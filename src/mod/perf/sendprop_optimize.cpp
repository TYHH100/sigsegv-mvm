#include "mod.h"
#include "util/scope.h"
#include "util/clientmsg.h"
#include "stub/baseplayer.h"
#ifdef SE_IS_TF2
#include "stub/tfplayer.h"
#include "stub/tfentities.h"
#endif
#include "stub/gamerules.h"
#include "stub/misc.h"
#include "stub/server.h"
#include "stub/baseweapon.h"
#ifdef SE_IS_TF2
#include "stub/tf_objective_resource.h"
#include "stub/tfweaponbase.h"
#endif
#include <forward_list>
#include "stub/sendprop.h"
#include "util/iterate.h"
#include "util/misc.h"
#include "util/thread_pool.h"
#include <link.h>
#include <bitset>

int global_frame_list_counter = 0;
bool is_client_hltv = false;

// Write offset that indicates that prop is absent and not written into the frame
#define PROP_WRITE_OFFSET_ABSENT 65535

// Invalid prop index for datatable 
#define INVALID_PROP_INDEX 65535

// IChangeFrameList implementation that avoids copying for less cpu/memory consumption
class CachedChangeFrameList : public IChangeFrameList
{
public:

    CachedChangeFrameList()
	{

	}

	void	Init( int nProperties, int iCurTick )
	{
		m_ChangeTicks.SetSize( nProperties );
		for ( int i=0; i < nProperties; i++ )
			m_ChangeTicks[i] = iCurTick;
        
	}


public:

	virtual void	Release()
	{
        m_CopyCounter--;
        if (m_CopyCounter < 0) {
            delete this;
        }
	}

	virtual IChangeFrameList* Copy()
	{
        m_CopyCounter++;
        return this;
	}

	virtual int		GetNumProps()
	{
		return m_ChangeTicks.Count();
	}

	virtual void	SetChangeTick( const int *pPropIndices, int nPropIndices, const int iTick )
	{
        bool same = (int)m_LastChangeTicks.size() == nPropIndices;
        m_LastChangeTicks.resize(nPropIndices);
		for ( int i=0; i < nPropIndices; i++ )
		{
            
            int prop = pPropIndices[i];
			m_ChangeTicks[ prop ] = iTick;
            
            same = same && m_LastChangeTicks[i] == prop;
            m_LastChangeTicks[i] = prop;

		}

        if (!same) {
            m_LastSameTickNum = iTick;
        }
        m_LastChangeTickNum = iTick;
        if (m_LastChangeTicks.capacity() > m_LastChangeTicks.size() * 8)
            m_LastChangeTicks.shrink_to_fit();
	}

	virtual int		GetPropsChangedAfterTick( int iTick, int *iOutProps, int nMaxOutProps )
	{
        int nOutProps = 0;
        if (iTick + 1 >= m_LastSameTickNum) {
            if (iTick >= m_LastChangeTickNum) {
                return 0;
            }

            nOutProps = m_LastChangeTicks.size();

            for ( int i=0; i < nOutProps; i++ )
            {
                iOutProps[i] = m_LastChangeTicks[i];
            }

            return nOutProps;
        }
        else {
            int c = m_ChangeTicks.Count();

            for ( int i=0; i < c; i++ )
            {
                if ( m_ChangeTicks[i] > iTick )
                {
                    iOutProps[nOutProps] = i;
                    ++nOutProps;
                }
            }
            return nOutProps;
        }
	}

// IChangeFrameList implementation.
protected:

	virtual			~CachedChangeFrameList()
	{
	}

private:
	// Change frames for each property.
	CUtlVector<int>		m_ChangeTicks;

    int m_LastChangeTickNum = 0;
    int m_LastSameTickNum = 0;
    int m_CopyCounter = 0;
	std::vector<int> m_LastChangeTicks;
};

struct carea_t
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;							// if two areas have equal floodnums, they are connected
	int		floodvalid;
};

class CCollisionBSPData
{
public:
    uint8_t pad[0x224];
    int numareas;
    carea_t *map_areas;
};
CBitVec<2048> *transmitAlways = nullptr;

NOINLINE void SetTransmitAlways(CServerNetworkProperty *netProp, bool bAlways) {
    auto edict = netProp->m_pPev;
    
    int parentIndex = netProp->m_hParent.GetEntryIndex();
    if ( bAlways || (parentIndex < MAX_EDICTS)) {
        transmitAlways->Set(edict->m_EdictIndex);
    }
    else if (edict != nullptr && edict->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION) {
        edict->m_fStateFlags &= ~FL_EDICT_DIRTY_PVS_INFORMATION;
        engine->BuildEntityClusterList(edict, &(netProp->m_PVSInfo));
    }
}

auto ourfunc1 = &SetTransmitAlways;

NOINLINE void SetTransmitParent(int parentIndex, CCheckTransmitInfo *pInfo, bool bAlways) {
    CBaseEntity *parent = (CBaseEntity *)(g_pWorldEdict+parentIndex)->GetUnknown();

    // Force our aiment and move parent to be sent.
    if (parent != nullptr) {
        parent->SetTransmit(pInfo, bAlways);
    }
}
auto ourfunc2 = &SetTransmitParent;

REPLACE_FUNC_MEMBER(void, CBaseEntity_SetTransmit, CCheckTransmitInfo *pInfo, bool bAlways)
{
    auto entity = reinterpret_cast<CBaseEntity *>(this);
    CServerNetworkProperty *netProp = entity->NetworkProp();
    int index = netProp->entindex();
    int parentIndex = netProp->m_hParent.GetEntryIndex();

    pInfo->m_pTransmitEdict->Set(index);

    if (transmitAlways) {
        (*ourfunc1)(netProp, bAlways);
    }
    if (parentIndex >= MAX_EDICTS || pInfo->m_pTransmitEdict->Get(parentIndex)) return;
    (*ourfunc2)(parentIndex, pInfo, bAlways);
}


IChangeInfoAccessor *world_accessor = nullptr;
CEdictChangeInfo *world_change_info = nullptr;
CSharedEdictChangeInfo *g_SharedEdictChangeInfo;

REPLACE_FUNC_MEMBER(IChangeInfoAccessor *, CBaseEdict_GetChangeAccessor)
{
    //world_accessor->SetChangeInfoSerialNumber(g_SharedEdictChangeInfo->m_iSerialNumber);
    //world_accessor->SetChangeInfo(0);
    //=0;
    g_SharedEdictChangeInfo->m_ChangeInfos[0].m_nChangeOffsets = 0;
    return world_accessor;
}

namespace Mod::Perf::SendProp_Optimize
{

    BS::thread_pool threadPool(1);
    BS::thread_pool threadPoolPackWork(1);

    SendTable *playerSendTable;
    ServerClass *playerServerClass;

    // key: prop offset, value: prop index

    struct PropIndexData
    {
        unsigned short offset = 0;
        unsigned short element = 0;
        unsigned short index1 = PROP_INDEX_INVALID;
        unsigned short index2 = PROP_INDEX_INVALID;
    };

    struct SpecialSendPropCalc
    {
        const int index;
    };
    struct SpecialDataTableCalc
    {
        std::vector<int> propIndexes;
        int baseOffset;
    };
    bool *player_local_exclusive_send_proxy;

    class ServerClassCache
    {
    public:
        std::vector<PropIndexData> prop_offset_sendtable;

        std::vector<SpecialSendPropCalc> prop_special;

        std::unordered_map<const SendProp *, SpecialDataTableCalc> datatable_special;

        unsigned short *prop_offsets;

        //CSendNode **send_nodes;

        // prop indexes that are stopped from being send to players
        unsigned char *prop_cull;

        // prop indexes that are stopped from being send to players
        unsigned short *prop_propproxy_first;
    };

    struct PropWriteOffset
    {
        unsigned short offset;
        unsigned short size;
    };
    // key: prop index value: write bit index
    std::vector<PropWriteOffset> prop_write_offset[MAX_EDICTS];

    std::vector<int> prop_value_old[MAX_EDICTS];
    unsigned short entity_frame_bit_size[MAX_EDICTS];

    //std::unordered_map<SendTable *, ServerClassCache> server_class_cache;
    ServerClassCache *player_class_cache = nullptr;

    SendTableProxyFn datatable_sendtable_proxy;
    SendTableProxyFn local_sendtable_proxy;

    edict_t * world_edict;
    RefCount rc_SendTable_WriteAllDeltaProps;

    inline ServerClassCache *GetServerClassCache(const SendTable *pTable)
    {
        return (ServerClassCache *)pTable->m_pPrecalc->m_pDTITable;
    }

    StaticFuncThunk<void, int , edict_t *, void *, CFrameSnapshot *> ft_SV_PackEntity("SV_PackEntity");
    StaticFuncThunk<IChangeFrameList*, int, int> ft_AllocChangeFrameList("AllocChangeFrameList");
    StaticFuncThunk<void, PackWork_t &> ft_PackWork_t_Process("PackWork_t::Process");
    StaticFuncThunk<bool, const SendTable *, const void *, bf_write *, int, CUtlMemory<CSendProxyRecipients> *, bool> ft_SendTable_Encode("SendTable_Encode");
    StaticFuncThunk<void, ServerClass *, int, const void *, int> ft_SV_EnsureInstanceBaseline("SV_EnsureInstanceBaseline");
    

    static inline void SV_PackEntity( 
        int edictIdx, 
        edict_t* edict, 
        void* pServerClass,
        CFrameSnapshot *pSnapshot )
    {
        ft_SV_PackEntity(edictIdx, edict, pServerClass, pSnapshot);
    }

    static inline void PackWork_t_Process( 
        PackWork_t &work )
    {
        ft_PackWork_t_Process(work);
    }

    static inline IChangeFrameList* AllocChangeFrameList(int props, int tick)
    {
        return ft_AllocChangeFrameList(props, tick);
    }

    static inline bool SendTable_Encode(const SendTable *table, const void *entity, bf_write *buf, int edictnum, CUtlMemory<CSendProxyRecipients> *recip, bool zeromem)
    {
        return ft_SendTable_Encode(table, entity, buf, edictnum, recip, zeromem);
    }

    static inline void SV_EnsureInstanceBaseline(ServerClass *serverClass, int edictnum, const void *data, int size)
    {
        return ft_SV_EnsureInstanceBaseline(serverClass, edictnum, data, size);
    }

    CStandardSendProxies* sendproxies;

    inline void CallProxyFn(SendProp *prop, unsigned char *base, DVariant &var, int entindex) {
        SendVarProxyFn func = prop->GetProxyFn();
        unsigned char *address = base + prop->GetOffset();
        if (func == sendproxies->m_Int32ToInt32) {
            var.m_Int = *((int *)address);
        }
        else if (func == sendproxies->m_UInt32ToInt32) {
            var.m_Int = *((uint *)address);
        }
        else if (func == sendproxies->m_Int16ToInt32) {
            var.m_Int = *((int16 *)address);
        }
        else if (func == sendproxies->m_UInt16ToInt32) {
            var.m_Int = *((uint16 *)address);
        }
        else if (func == sendproxies->m_Int8ToInt32) {
            var.m_Int = *((int8 *)address);
        }
        else if (func == sendproxies->m_UInt8ToInt32) {
            var.m_Int = *((uint8 *)address);
        }
        else if (func == sendproxies->m_FloatToFloat) {
            var.m_Float = *((float *)address);
        }
        else if (func == sendproxies->m_VectorToVector) {
            auto &vec = *((Vector *)address);
            var.m_Vector[0] = vec[0];
            var.m_Vector[1] = vec[1];
            var.m_Vector[2] = vec[2];
        }
        else if (func == DLLSendProxy_VectorXYToVectorXY.GetRef()) {
            auto &vec = *((Vector *)address);
            var.m_Vector[0] = vec[0];
            var.m_Vector[1] = vec[1];
        }
        else if (func == DLLSendProxy_QAngles.GetRef()) {
            auto &vec = *((QAngle *)address);
            var.m_Vector[0] = anglemod(vec.x);
            var.m_Vector[1] = anglemod(vec.y);
            var.m_Vector[2] = anglemod(vec.z);
        }
        else if (func == DLLSendProxy_StringToString.GetRef()) {
            var.m_pString = (const char *)address;
        }
        else if (func == DLLSendProxy_AngleToFloat.GetRef()) {
            var.m_Float = anglemod(*((float *)address));
        }
        else if (func == DLLSendProxy_Color32ToInt.GetRef()) {
            color32 *pIn = (color32*)address;
	        var.m_Int = (int)(((unsigned int)pIn->r << 24) | ((unsigned int)pIn->g << 16) | ((unsigned int)pIn->b << 8) | ((unsigned int)pIn->a));
        }
        else if (func == DLLSendProxy_StringT_To_String.GetRef()) {
            var.m_pString = STRING(*((string_t*)address));
        }
        else if (func == DLLSendProxy_EHandleToInt.GetRef()) {
            CBaseHandle *pHandle = (CBaseHandle*)address;

            if (pHandle && pHandle->Get()) {
                int iSerialNum = pHandle->GetSerialNumber() & ((1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1);
                var.m_Int = pHandle->GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS);
            }
            else {
                var.m_Int = INVALID_NETWORKED_EHANDLE_VALUE;
            }
        }
        else if (func == DLLSendProxy_Empty.GetRef()) {

        }
        else {
            func( 
                prop,
                base, 
                base + prop->GetOffset(), 
                &var, 
                0, // iElement
                entindex
                );
        }
    }

    inline bool AreBitsDifferent(bf_read *bf1, bf_read *bf2, int bits)
    {
        int dWords = bits >> 5;
        for (int i = 0; i < dWords; i++) {
            uint bits1 = bf1->ReadUBitLong(32);
            uint bits2 = bf2->ReadUBitLong(32);
            if (bits1 != bits2) {
                return true;
            }
        }
        int remainder = bits - (dWords << 5);
        return remainder != 0 && bf1->ReadUBitLong(remainder) != bf2->ReadUBitLong(remainder);
    }

    int count_tick;
    int recursecount = 0;
    CSendProxyRecipients static_recip;
    inline bool RecurseStackRecipients(unsigned short *propPropProxy, PropWriteOffset *propWriteOffset, int entindex, CSendTablePrecalc *precalc, unsigned char *base, CUtlMemory<CSendProxyRecipients> *recipients, CSendNode *node) {
        bool needFullReload = false;
        for (int i = 0; i < node->m_Children.Count(); i++) {
            CSendNode *child = node->m_Children[i];

            CSendProxyRecipients *recipientsS;
            if (child->m_DataTableProxyIndex != DATATABLE_PROXY_INDEX_NOPROXY) {
                
                recipientsS = &recipients->Element(child->m_DataTableProxyIndex);
                recipientsS->SetAllRecipients();
            }
            else {
                recipientsS = &static_recip;
            }
            if (child->m_Children.IsEmpty() && child->m_DataTableProxyIndex == DATATABLE_PROXY_INDEX_NOPROXY) continue;
            auto prop = precalc->m_DatatableProps[child->m_iDatatableProp];

            auto dataTableFn = prop->GetDataTableProxyFn();
            unsigned char *newBase = nullptr;
            if (base != nullptr) {
                newBase = (unsigned char*)prop->GetDataTableProxyFn()( 
                    prop,
                    base, 
                    base + prop->GetOffset(), 
                    recipientsS,
                    entindex

                    );
            }

            if (child->m_DataTableProxyIndex < precalc->m_nDataTableProxies) {
                int firstPropIndex = propPropProxy[child->m_DataTableProxyIndex];
                if (firstPropIndex != INVALID_PROP_INDEX && firstPropIndex >= 0 && firstPropIndex < precalc->m_Props.Count()) {
                    bool prevWritten = propWriteOffset[firstPropIndex].offset != PROP_WRITE_OFFSET_ABSENT;

                    if ((newBase != nullptr && !prevWritten) || (newBase == nullptr && prevWritten)) {
                        return true;
                    }
                }
            }

            if (!child->m_Children.IsEmpty() && newBase != nullptr)
                needFullReload |= RecurseStackRecipients(propPropProxy, propWriteOffset, entindex, precalc, newBase, recipients, child);
        }
        return needFullReload;
    }

    std::mutex createPackedEntityMutex;
    
    bool firstPack = true;
    int threadNum = 0;
    inline bool IsParallel() {
        return threadNum > 0 && !firstPack;
    }

    CSendProxyRecipients recip;
    int edictnotfullchanged = 0;
    thread_local int concurrencyPackIndex = -1;
    static inline bool DoEncode(ServerClass *serverclass, ServerClassCache &cache, edict_t *edict, int objectID, CFrameSnapshot *snapshot) {
        CBaseEntity *entity = (CBaseEntity *)edict->GetUnknown();
        //not_force_updated[objectID] = false;

        SendTable *pTable = serverclass->m_pTable;
        if (prop_value_old[objectID].size() != cache.prop_offset_sendtable.size()) {
            prop_value_old[objectID].resize(cache.prop_offset_sendtable.size());
        }
        
        int offsetcount = cache.prop_offset_sendtable.size();
        auto old_value_data = prop_value_old[objectID].data();
        //DevMsg("crash3\n");

        int propOffsets[100];
        int propOffsetsElement[100];
        int propChangeOffsets = 0;

        char * pStruct = (char *)edict->GetUnknown();

        // Remember last write offsets 
        // player_prop_write_offset_last[objectID - 1] = player_prop_write_offset[objectID - 1];
        auto &player_prop_cull = player_class_cache->prop_cull;
        auto isPlayer = serverclass == playerServerClass;
        bool bot = isPlayer && entity->GetFlags() & FL_FAKECLIENT;

        for (int i = 0; i < offsetcount; i++) {
            PropIndexData &data = cache.prop_offset_sendtable[i];
            int valuepre = old_value_data[i];
            int valuepost = *(int*)(pStruct + data.offset);
            if (valuepre != valuepost) {
                if (propChangeOffsets >= 100) {
                    return false;
                }
                if (propChangeOffsets && (propOffsets[propChangeOffsets - 1] == data.index1 || propOffsets[propChangeOffsets - 1] == data.index2))
                    continue;

                old_value_data[i] = valuepost;
                if (!(bot && player_prop_cull[data.index1] < 254 && player_local_exclusive_send_proxy[player_prop_cull[data.index1]])) {
                    propOffsetsElement[propChangeOffsets] = data.element;
                    propOffsets[propChangeOffsets++] = data.index1;
                }

                if (data.index2 != PROP_INDEX_INVALID && !(bot && player_prop_cull[data.index2] < 254 && player_local_exclusive_send_proxy[player_prop_cull[data.index2]])) {
                    propOffsets[propChangeOffsets++] = data.index2;
                }
                
                //DevMsg("Detected value change prop vector %d %d %s\n", vec_prop_offsets[j], i, pPrecalc->m_Props[i]->GetName());
            }
        }
        // for (auto &prop : prop_special) {
        //     void *data = pStruct;
        //     Msg("Data pre %d %d %d %s\n",prop.baseoffset, data, pStruct, prop.prop->GetName());
        //     if (prop.datatable != nullptr) {
        //         data = prop.datatable->GetDataTableProxyFn()(prop.datatable, pStruct + prop.baseoffset, pStruct + prop.baseoffset + prop.datatable->GetOffset(), &recip, objectID);
        //     }
        //     Msg("Data post %d %d %s\n", data, pStruct, prop.prop->GetName());
        //     DVariant var;
        //     var.m_Type = (SendPropType)prop.prop->m_Type;
        //     prop.prop->GetProxyFn()( 
        //                     prop.prop,
        //                     pStruct, 
        //                     data + prop.prop->GetOffset(), 
        //                     &var, 
        //                     0, // iElement
        //                     objectID
        //                     );
        // }

        if ((int) prop_write_offset[objectID].size() != pTable->m_pPrecalc->m_Props.Count() + 1) {
            prop_write_offset[objectID].resize(pTable->m_pPrecalc->m_Props.Count() + 1);
            return false;
        }
        CFrameSnapshotManager &snapmgr = g_FrameSnapshotManager;
        if (propChangeOffsets == 0 && snapmgr.UsePreviouslySentPacket(snapshot, objectID, edict->m_NetworkSerialNumber)) {
            if (!IsParallel())
                edict->m_fStateFlags &= ~(FL_EDICT_CHANGED | FL_FULL_EDICT_CHANGED);
            return true;
        }
        auto write_offset_data = prop_write_offset[objectID].data();

        PackedEntity *pPrevFrame = snapmgr.GetPreviouslySentPacket( objectID, edict->m_NetworkSerialNumber /*snapshot->m_pEntities[ edictnum ].m_nSerialNumber*/ );
        
        if (pPrevFrame != nullptr) {
            CSendTablePrecalc *pPrecalc = pTable->m_pPrecalc;

            //Copy previous frame data
            const void *oldFrameData = pPrevFrame->GetData();
            unsigned short &frameDataLength = entity_frame_bit_size[objectID];
            
            ALIGN4 char packedData[8192] ALIGN4_POST;
            memcpy(packedData, oldFrameData, pPrevFrame->GetNumBytes());

            bf_write prop_writer("packed data writer", packedData, sizeof(packedData));
            bf_read prop_reader("packed data reader", packedData, sizeof(packedData));
            IChangeFrameList *pChangeFrame = NULL;

            unsigned char sizetestbuf[DT_MAX_STRING_BUFFERSIZE+2];
            bf_write sizetestbuf_write("packed data writer", sizetestbuf, sizeof(sizetestbuf));
            bf_read sizetestbuf_read("packed data reader", sizetestbuf, sizeof(sizetestbuf));
            
            int propsChanged[100];
            int propsChangedCount = 0;
            PropTypeFns *encode_fun = g_PropTypeFns;
            //{
            //    TIME_SCOPE2(encodeprop);
            // Insertion sort on prop indexes 
            int i = 1;
            while (i < propChangeOffsets) {
                int x = propOffsets[i];
                int j = i - 1;
                while (j >= 0 && propOffsets[j] > x) {
                        propOffsets[j+1] = propOffsets[j];
                    j = j - 1;
                }
                propOffsets[j+1] = x;
                i = i + 1;
            }
            //DevMsg("offsets %d %d\n", propChangeOffsets, p->m_nChangeOffsets);

            //int bit_offsetg = player_prop_write_offset[objectID - 1][player_prop_write_offset[objectID - 1].size()-1];
            //DevMsg("max write offset %d %d\n", bit_offsetg, propChangeOffsets);

            
            for (int i = 0; i < propChangeOffsets; i++) {
                
                //DevMsg("prop %d %d\n", i, propOffsets[i]);
                int propId = propOffsets[i];
                int bit_offset = write_offset_data[propId].offset;
                if (bit_offset == 65535) {
                    continue;
                }

                //prop_reader.Seek(bit_offset);
                DVariant var;
                const SendProp *pProp = pPrecalc->m_Props[propId];

                //DevMsg("max write offset %d %s %d\n", propOffsets[i], pProp->GetName(), bit_offset);


                char *pStructBaseOffset;
                pStructBaseOffset = pStruct + cache.prop_offsets[propId] - pProp->GetOffset();

                var.m_Type = (SendPropType)pProp->m_Type;
                if (var.m_Type != DPT_Array) {
                    pProp->GetProxyFn()( 
                        pProp,
                        pStructBaseOffset, 
                        pStructBaseOffset + pProp->GetOffset(), 
                        &var, 
                        0, // iElement
                        objectID
                        );
                }

                //DevMsg("prop %d %d %d %s %s\n",player_prop_offsets[propOffsets[i]], propOffsets[i], pStructBaseOffset + pProp->GetOffset(),pProp->GetName(), var.ToString());

                // DecodeInfo decodeInfo;
                // decodeInfo.m_pRecvProp = nullptr; // Just skip the data if the proxies are screwed.
                // decodeInfo.m_pProp = pProp;
                // decodeInfo.m_pIn = &prop_reader;
                // decodeInfo.m_ObjectID = objectID;
                // decodeInfo.m_Value.m_Type = (SendPropType)pProp->m_Type;
                // encode_fun[pProp->m_Type].Decode(&decodeInfo);

                sizetestbuf_write.SeekToBit(0);
                encode_fun[pProp->m_Type].Encode( 
                    (const unsigned char *) pStructBaseOffset, 
                    &var, 
                    pProp, 
                    &sizetestbuf_write, 
                    objectID
                    ); 
            
                int bit_size = write_offset_data[propId].size;

                int bit_offset_change = sizetestbuf_write.GetNumBitsWritten() - (bit_size);

                // Move all bits left or right
                if (bit_offset_change != 0) {
                        // Msg("offset change %d %s %d %s %d %d oldSize: %d new: %d\n", bit_offset_change, entity->GetClassname(), propOffsets[i], pProp->GetName(), prop_writer.GetNumBitsWritten(), bit_size + bit_offset, bit_size, sizetestbuf_write.GetNumBitsWritten());
                        
                    if (bit_offset_change < 0) {
                        prop_reader.Seek(bit_offset + bit_size);
                        prop_writer.SeekToBit(bit_offset + sizetestbuf_write.GetNumBitsWritten());
                        prop_writer.WriteBitsFromBuffer(&prop_reader, frameDataLength - prop_reader.m_iCurBit);
                    }
                    else {
                        
                        for (int j = frameDataLength; j >= bit_offset + bit_size;) {
                            int bitsRead = Min(j - (bit_offset + bit_size), 32);
                            if (bitsRead <= 0) break;

                            j-=bitsRead;
                            prop_reader.Seek(j);
                            uint var = prop_reader.ReadUBitLong(bitsRead);
                            prop_writer.SeekToBit(j + bit_offset_change);
                            prop_writer.WriteUBitLong(var, bitsRead);
                        }
                    }
                    int propcount = pPrecalc->m_Props.Count();
                    write_offset_data[propId].size = sizetestbuf_write.GetNumBitsWritten();
                    for (int j = propId + 1; j < propcount; j++) {
                        if (write_offset_data[j].offset != PROP_WRITE_OFFSET_ABSENT)
                            write_offset_data[j].offset += bit_offset_change;
                    }
                    
                    frameDataLength+=bit_offset_change;
                }
                prop_reader.Seek(bit_offset);
                sizetestbuf_read.Seek(0);
                
                if (bit_offset_change != 0 || AreBitsDifferent(&prop_reader, &sizetestbuf_read, bit_size)) {
                    
                    prop_writer.SeekToBit(bit_offset);
                    sizetestbuf_read.Seek(0);
                    prop_writer.WriteBitsFromBuffer(&sizetestbuf_read, sizetestbuf_write.GetNumBitsWritten());
                    propsChanged[propsChangedCount++] = propId;
                }
            }
            //}

            unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
            CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pTable->m_pPrecalc->m_nDataTableProxies );
            bool needFullReload = RecurseStackRecipients(cache.prop_propproxy_first, write_offset_data, objectID, pTable->m_pPrecalc, (unsigned char *)pStruct, &recip, &pTable->m_pPrecalc->m_Root);
            if (needFullReload) {
                return false;
            }

            if (hltv != nullptr && hltv->IsActive()) {
                pChangeFrame = pPrevFrame->m_pChangeFrameList;
                pChangeFrame = pChangeFrame->Copy();
            }
            else {
                pChangeFrame = pPrevFrame->SnagChangeFrameList();
            } 

            pChangeFrame->SetChangeTick( propsChanged, propsChangedCount, snapshot->m_nTickCount );
            SV_EnsureInstanceBaseline( serverclass, objectID, packedData, Bits2Bytes(frameDataLength) );
            //CUtlMemory< CSendProxyRecipients > recip(pPrevFrame->GetRecipients(), pTable->m_pPrecalc->m_nDataTableProxies );

            {
                //Msg("Snap thread %d %d %d %d %d %d\n", snapshot, objectID, snapmgr.m_PackedEntitiesPool.Count(), snapmgr.m_PackedEntitiesPool.PeakCount(), snapmgr.m_PackedEntitiesPool.m_pHeadOfFreeList, &snapmgr.m_PackedEntitiesPool.m_pHeadOfFreeList);
                PackedEntity *pPackedEntity = snapmgr.CreatePackedEntity( snapshot, objectID );
                pPackedEntity->m_pChangeFrameList = pChangeFrame;
                pPackedEntity->SetServerAndClientClass( serverclass, NULL );
                pPackedEntity->AllocAndCopyPadded( packedData, Bits2Bytes(frameDataLength) );
                pPackedEntity->SetRecipients( recip );
                
            }
            if (!IsParallel())
                edict->m_fStateFlags &= ~(FL_EDICT_CHANGED | FL_FULL_EDICT_CHANGED);

            //not_force_updated[objectID] = true;
            return true;
        }
        return false;
    }
    RefCount rc_CHLTVClient_SendSnapshot;
    DETOUR_DECL_MEMBER(void, CHLTVClient_SendSnapshot, CClientFrame *frame) {
        SCOPED_INCREMENT(rc_CHLTVClient_SendSnapshot);
        DETOUR_MEMBER_CALL(CHLTVClient_SendSnapshot)(frame);
    }
    DETOUR_DECL_STATIC(void, SendTable_WritePropList,
        const SendTable *pTable,
        const void *pState,
        const int nBits,
        bf_write *pOut,
        const int objectID,
        const int *pCheckProps,
        const int nCheckProps
        )
    {
        if (rc_CHLTVClient_SendSnapshot == 0 && objectID >= 0) {
            if ( nCheckProps == 0 ) {
                // Write single final zero bit, signifying that there no changed properties
                pOut->WriteOneBit( 0 );
                return;
            }
            if (prop_write_offset[objectID].empty()) {
                DETOUR_STATIC_CALL(SendTable_WritePropList)(pTable, pState, nBits, pOut, objectID, pCheckProps, nCheckProps);
                return;
            }
            CDeltaBitsWriter deltaBitsWriter( pOut );
	        bf_read inputBuffer( "SendTable_WritePropList->inputBuffer", pState, BitByte( nBits ), nBits );

            auto pPrecalc = pTable->m_pPrecalc;
            auto offset_data = prop_write_offset[objectID].data();
            for (int i = 0; i < nCheckProps; i++) {
                int propid = pCheckProps[i];
                int offset = offset_data[propid].offset;
                if (offset == 0 || offset == PROP_WRITE_OFFSET_ABSENT)
                    continue;
                

			    deltaBitsWriter.WritePropIndex(propid);

                int len = offset_data[propid].size;
                inputBuffer.Seek(offset);
                pOut->WriteBitsFromBuffer(&inputBuffer, len);
            }
                
            return;
        }
        DETOUR_STATIC_CALL(SendTable_WritePropList)(pTable, pState, nBits, pOut, objectID, pCheckProps, nCheckProps);
    }

    DETOUR_DECL_STATIC(int, SendTable_CullPropsFromProxies,
	const SendTable *pTable,
	
	const int *pStartProps,
	int nStartProps,

	const int iClient,
	
	const CSendProxyRecipients *CullPropsFromProxies,
	const int nOldStateProxies, 
	
	const CSendProxyRecipients *pNewStateProxies,
	const int nNewStateProxies,
	
	int *pOutProps,
	int nMaxOutProps
	)
    {
        //memcpy(pOutProps, pStartProps, nStartProps * sizeof(int));
        int count = 0;
        auto pPrecalc = pTable->m_pPrecalc;
        auto &prop_cull = GetServerClassCache(pTable)->prop_cull;
        for (int i = 0; i <nStartProps; i++) {
            int prop = pStartProps[i];
            //DevMsg("prop %d %d", prop, player_prop_cull[prop]);
            int proxyindex = prop_cull[prop];
            //DevMsg("%s", pPrecalc->m_Props[prop]->GetName());
            if (proxyindex < 254 ) {
                //DevMsg("node %s\n", player_send_nodes[proxyindex]->m_pTable->GetName());
                if (pNewStateProxies[proxyindex].m_Bits.IsBitSet(iClient)) {
                    pOutProps[count++] = prop;
                }
            }
            else {
                //DevMsg("node none\n");
                pOutProps[count++] = prop;
            }
        }
        return count;
        //
    }

    uintptr_t packinfoOffset = 0;
    
    void PackWork(PackWork_t *work, size_t items)
    {
        /*CFastTimer timer;
        maxthreads = cvar_threads.GetInt();
        timer.Start();
        for (int i = 0; i < items; i++) {
            SV_PackEntity(work->nIdx, work->pEdict, work->pSnapshot->m_pEntities[ work->nIdx ].m_pClass, work->pSnapshot );
            work++;
        }
        timer.End();*/

        //DevMsg("hmm\n");

        //std::vector<PackWork_t> work_do;

        //CFastTimer timer;
        //timer.Start();
        VPROF_BUDGET("CParallelProcessor<PackWork_t>::Run", "Encoding entities");
        int max_players = gpGlobals->maxClients;
        CFrameSnapshotManager &snapmgr = g_FrameSnapshotManager;
        bool parallel = IsParallel();

        for (size_t i = 0; i < items; i++) {
            PackWork_t *work_i = work + i;
            edict_t *edict = work_i->pEdict;
            int objectID = work_i->nIdx;
            //if (objectID > max_players) {
            //   player_index_end = i;
            //    break;
            //}

            if (!(edict->m_fStateFlags & FL_EDICT_CHANGED)) {
                if (snapmgr.UsePreviouslySentPacket(work_i->pSnapshot, objectID, edict->m_NetworkSerialNumber)) {
                    continue;
                }
            }
            
            //pack[concurrencyPackIndex]++;
            CBaseEntity *entity = (CBaseEntity *)edict->GetUnknown();
            ServerClass *serverclass =  entity->NetworkProp()->GetServerClass();
            ServerClassCache &cache = *GetServerClassCache(serverclass->m_pTable);
            
            if (!(edict->m_fStateFlags & FL_FULL_EDICT_CHANGED)) {
                bool lastFullEncode = !DoEncode(serverclass, cache, edict, objectID, work_i->pSnapshot );
                if (!lastFullEncode) {

                    continue;
                }
                
            }
            if (!parallel)
                edict->m_fStateFlags |= FL_EDICT_CHANGED;

            PackWork_t_Process(*work_i);

            // Update player prop write offsets
            
            PackedEntity *pPrevFrame = snapmgr.GetPreviouslySentPacket( objectID, edict->m_NetworkSerialNumber /*snapshot->m_pEntities[ edictnum ].m_nSerialNumber*/ );
            if (pPrevFrame != nullptr) {

                uint offsetcount = cache.prop_offset_sendtable.size();
                if (prop_value_old[objectID].size() != offsetcount) {
                    prop_value_old[objectID].resize(offsetcount);
                }
                auto old_value_data = prop_value_old[objectID].data();
                auto pStruct = (char *)edict->GetUnknown();
                for (uint i = 0; i < offsetcount; i++) {
                    PropIndexData &data = cache.prop_offset_sendtable[i];
                    old_value_data[i] = *(int*)(pStruct + data.offset);
                }

                SendTable *pTable = serverclass->m_pTable;
                int propcount = pTable->m_pPrecalc->m_Props.Count();
                if ((int) prop_write_offset[objectID].size() != propcount + 1) {
                    prop_write_offset[objectID].resize(propcount + 1);
                }

                bf_read toBits( "SendTable_CalcDelta/toBits", pPrevFrame->GetData(), BitByte(pPrevFrame->GetNumBits()), pPrevFrame->GetNumBits() );
                CDeltaBitsReader toBitsReader( &toBits );

                unsigned int iToProp = toBitsReader.ReadNextPropIndex();
                
                auto offset_data = prop_write_offset[objectID].data();
                for (uint i = 0; i < iToProp && iToProp < MAX_DATATABLE_PROPS; i++) {
                    offset_data[i].offset = PROP_WRITE_OFFSET_ABSENT;
                }
                
                // Required for later writeproplist
                offset_data[propcount].offset = pPrevFrame->GetNumBits() + 7;

                int lastbit = 0;
                int lastprop = -1;
                for ( ; iToProp < MAX_DATATABLE_PROPS; iToProp = toBitsReader.ReadNextPropIndex())
                { 
                    const SendProp *pProp = pTable->m_pPrecalc->m_Props[iToProp];
                    if ((int)iToProp != lastprop + 1) {
                        for (int i = lastprop + 1; i < (int) iToProp; i++) {
                            offset_data[i].offset = PROP_WRITE_OFFSET_ABSENT;
                            //Msg("Set offset data %s %d to invalid\n", pTable->GetName(), i);
                        }
                    }
                    int write_offset = toBits.GetNumBitsRead();
                    //if (toBits.GetNumBitsRead() - lastbit != 7) {
                    //    write_offset |= PROP_INDEX_WRITE_LENGTH_NOT_7_BIT;
                    //}

                    offset_data[iToProp].offset = write_offset;

                    toBitsReader.SkipPropData(pProp);
                    int lastbitbefore = lastbit;
                    lastbit = toBits.GetNumBitsRead();
                    offset_data[iToProp].size = lastbit - write_offset;

                    //Msg("Prop %d %s propdatasize: %d nextpropindexsize: %d\n", iToProp, entity->GetClassname(), lastbit - write_offset, write_offset - lastbitbefore);
                    lastprop = iToProp; 
                }
                for (lastprop++; lastprop < propcount; lastprop++) {
                    offset_data[lastprop].offset = PROP_WRITE_OFFSET_ABSENT;
                }
                entity_frame_bit_size[objectID] = toBits.GetNumBitsRead();
            }
        }
        //Msg("Pack t %d %d %d %d\n", pack[0], pack[1], pack[2], pack[3]);
        //Msg("Pack time %.9f %.9f %.9f %.9f %.9f\n", time.GetSeconds(), time2.GetSeconds(), time3.GetSeconds(), time4.GetSeconds(), time5.GetSeconds());
        //timer.End();
        //DevMsg("Timer encode players %.9f\n", timer.GetDuration().GetSeconds());
        // for (int i = player_index_end; i < items; i++) {
        //     PackWork_t *work_i = work + i;
        //     edict_t *edict = work_i->pEdict;
        //     int objectID = work_i->nIdx;
        //     if (!(edict->m_fStateFlags & FL_EDICT_CHANGED)) {
        //         bool send = snapmgr.UsePreviouslySentPacket(work_i->pSnapshot, objectID, edict->m_NetworkSerialNumber);
        //         if (send) {
        //             continue;
        //         }
        //     }
        //     //CFastTimer timer2;
        //     //timer2.Start();
        //     edict->m_fStateFlags |= FL_EDICT_CHANGED;
        //     PackWork_t_Process(*work_i);
        //     //timer2.End();
        //     //DevMsg("Timer encode other %s %.9f\n", GetContainingEntity(edict)->GetClassname() ,timer2.GetDuration().GetSeconds());
        // }
        //DETOUR_MEMBER_CALL(CParallelProcessor_PackWork_t_Run)(work_do.data(), work_do.size(), maxthreads, pool);
        //DevMsg("duration for %d %f\n", maxthreads, timer.GetDuration().GetSeconds());
    }

    DETOUR_DECL_MEMBER(void, CParallelProcessor_PackWork_t_Run, PackWork_t *work, long items, long maxthreads, void *pool)
	{
        if (IsParallel()) {
            return;
        }

        PackWork(work, items);
    }

    RefCount rc_SV_ComputeClientPacks;
    
    DETOUR_DECL_MEMBER(void, CGameClient_SetupPackInfo, CFrameSnapshot *snapshot)
	{
        if (rc_SV_ComputeClientPacks) return;

        DETOUR_MEMBER_CALL(CGameClient_SetupPackInfo)(snapshot);
    }


    DETOUR_DECL_MEMBER(void, CGameClient_SetupPrevPackInfo)
	{
        if (rc_SV_ComputeClientPacks) return;

        DETOUR_MEMBER_CALL(CGameClient_SetupPrevPackInfo)();
    }

    uint16_t *areasConnected = nullptr;
    GlobalThunk<CCollisionBSPData> g_BSPData("g_BSPData");
    inline bool CheckAreasConnected(int area1, int area2)
    {
        if (areasConnected != nullptr) {
            return areasConnected[area1] == areasConnected[area2];
        }
        auto &BSPData = g_BSPData.GetRef();
        return BSPData.map_areas[area1].floodnum == BSPData.map_areas[area2].floodnum;
    }

    inline bool IsInPVS(CServerNetworkProperty *prop, CCheckTransmitInfo *pInfo)
    {
        int i;
        auto &m_PVSInfo = prop->m_PVSInfo;
        if (!m_PVSInfo.m_nAreaNum2) {
            for ( i=0; i< pInfo->m_AreasNetworked; i++ ) {
                int clientArea = pInfo->m_Areas[i];
                if ( clientArea == m_PVSInfo.m_nAreaNum || CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum ) )
                    break;
            }
        }
        else {
            // doors can legally straddle two areas, so
            // we may need to check another one
            for ( i=0; i< pInfo->m_AreasNetworked; i++ )
            {
                int clientArea = pInfo->m_Areas[i];
                if ( clientArea == m_PVSInfo.m_nAreaNum || clientArea == m_PVSInfo.m_nAreaNum2 )
                    break;

                if ( CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum ) )
                    break;

                if ( CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum2 ) )
                    break;
            }
        }

        if (i == pInfo->m_AreasNetworked) {
            return false;
        }

        unsigned char *pPVS = ( unsigned char * )pInfo->m_PVS;
        
        if ( m_PVSInfo.m_nClusterCount < 0 ) {
            return (engine->CheckHeadnodeVisible( m_PVSInfo.m_nHeadNode, pPVS, pInfo->m_nPVSSize ) != 0);
        }
        
        for ( i = m_PVSInfo.m_nClusterCount; --i >= 0; ) {
            int nCluster = m_PVSInfo.m_pClusters[i];
            if ( ((int)(pPVS[nCluster >> 3])) & BitVec_BitInByte( nCluster ) )
                return true;
        }

        return false;
    }

    bool firstTransmit = false;
    DETOUR_DECL_MEMBER(void, CServerGameEnts_CheckTransmit, CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts)
	{
        if (packinfoOffset == 0) {
            packinfoOffset = (uintptr_t)pInfo - (uintptr_t)static_cast<CBaseClient *>(sv->GetClient(pInfo->m_pClientEnt->m_EdictIndex - 1));
        }

        if (rc_SV_ComputeClientPacks) {
            return;
        }
        
        static unsigned short edictsCheck[MAX_EDICTS];
        static int nEdictsCheck = 0;
        static unsigned short edictsAlwaysSend[MAX_EDICTS];
        static int nEdictsAlwaysSend = 0;
        auto player = (CBasePlayer *) GetContainingEntity(pInfo->m_pClientEnt);
        if (player == nullptr) return;
        
        bool isTV = player->IsReplay() || player->IsHLTV();
        int skyBoxArea = player->m_Local->m_skybox3darea;
        transmitAlways = pInfo->m_pTransmitAlways;
        if (firstTransmit) {
            std::bitset<MAX_EDICTS> addedToList;
            firstTransmit = false;
            nEdictsCheck = 0;
            nEdictsAlwaysSend = 0;
            std::vector<int> list;
            for (auto i = 0; i < nEdicts; i++) {
                uint16_t index = pEdictIndices[i];
                if (addedToList.test(index)) continue;
                edict_t *edict = g_pWorldEdict + index;
                int nFlags = edict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);

                CBaseEntity *pEnt = ( CBaseEntity * )edict->GetUnknown();

                if (nFlags & FL_EDICT_DONTSEND) {
                    addedToList.set(index);
                    continue;
                }
                if (nFlags & FL_EDICT_ALWAYS) {
                    while(!pInfo->m_pTransmitEdict->Get(index)) {
                        addedToList.set(index);
                        pInfo->m_pTransmitEdict->Set(index);
                        if (isTV) {
                            pInfo->m_pTransmitAlways->Set(index);
                        }
                        edictsAlwaysSend[nEdictsAlwaysSend++] = index;
                        auto networkable = (CServerNetworkProperty *) edict->GetNetworkable();
                        if (networkable == nullptr) break;
                        auto hParent = networkable->m_hParent.Get();
                        if (hParent == nullptr) break;
                        edict = hParent->edict();
                        index = hParent->entindex();
                    }
                    continue;
                }
                list.clear();
                int origIndex = index;
                while (edict != nullptr) {
                    addedToList.set(index);
                    auto networkable = (CServerNetworkProperty *) edict->GetNetworkable();
                    if (networkable == nullptr) break;
                    auto hParent = networkable->m_hParent.Get();
                    if (hParent == nullptr) break;
                    edict = hParent->edict();
                    index = hParent->entindex();
                    if (addedToList.test(index)) break;
                    list.push_back(index);
                }
                for (auto it = list.rbegin(); it != list.rend(); it++) {
                    edictsCheck[nEdictsCheck++] = *it;
                }
                edictsCheck[nEdictsCheck++] = origIndex;
            }
        }
        else {
            for (auto i = 0; i < nEdictsAlwaysSend; i++) {
                uint16_t index = edictsAlwaysSend[i];
                edict_t *edict = g_pWorldEdict + index;
                pInfo->m_pTransmitEdict->Set(index);
                if (isTV) {
                    pInfo->m_pTransmitAlways->Set(index);
                }
            }
        }

        for (auto i = 0; i < nEdictsCheck; i++) {
            uint16_t index = edictsCheck[i];
            if (pInfo->m_pTransmitEdict->Get(index)) continue;
            edict_t *edict = g_pWorldEdict + index;
            int nFlags = edict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);
            CBaseEntity *pEnt = ( CBaseEntity * )edict->GetUnknown();

            if ( nFlags == FL_EDICT_FULLCHECK ) {
                nFlags = pEnt->ShouldTransmit( pInfo );

                if ( nFlags & FL_EDICT_ALWAYS ) {
                    
                    pEnt->SetTransmit( pInfo, true );
                    continue;
                }
            }

            if ( !( nFlags & FL_EDICT_PVSCHECK ) )
                continue;
            
            CServerNetworkProperty *netProp = static_cast<CServerNetworkProperty*>( edict->GetNetworkable() );

            // Always send entities in the player's 3d skybox.
            // Sidenote: call of AreaNum() ensures that PVS data is up to date for this entity
            if (edict->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION) {
                edict->m_fStateFlags &= ~FL_EDICT_DIRTY_PVS_INFORMATION;
                engine->BuildEntityClusterList(edict, &(netProp->m_PVSInfo));
            }
            int areanum = netProp->m_PVSInfo.m_nAreaNum;

            if (areanum == skyBoxArea) {
                pEnt->SetTransmit( pInfo, true);
                continue;
            }

            if (isTV || IsInPVS(netProp, pInfo)) {
                // only send if entity is in PVS
                pEnt->SetTransmit( pInfo, false );
                continue;
            }

            int parentIndex = netProp->m_hParent.GetEntryIndex();
            if (parentIndex <= MAX_EDICTS && pInfo->m_pTransmitEdict->Get(parentIndex)) {
                pEnt->SetTransmit( pInfo, true );
                continue;
            }
        }
        //return DETOUR_MEMBER_CALL(CServerGameEnts_CheckTransmit)(pInfo, pEdictIndices, nEdicts);

    }

    std::atomic<bool> computedPackInfos[ABSOLUTE_PLAYER_LIMIT];
    std::atomic<bool> checkTransmitComplete[ABSOLUTE_PLAYER_LIMIT];

    std::atomic<bool> packWorkFinished = false;
    std::atomic<bool> setupPackInfoFinished = false;
    
    DETOUR_DECL_MEMBER(bool, CVEngineServer_CheckAreasConnected, int area1, int area2)
	{
        if (areasConnected != nullptr) {
            return areasConnected[area1] == areasConnected[area2];
        }
        return DETOUR_MEMBER_CALL(CServerGameEnts_CheckTransmit)(area1, area2);
    }

    DETOUR_DECL_STATIC(void, SV_ComputeClientPacks, int clientCount,  CGameClient **clients, CFrameSnapshot *snapshot)
	{
        // Do not network dead bots and reduce update rate for their weapons
        for (int i = 1; i <= gpGlobals->maxClients; i++) {
            edict_t *edict = world_edict + i;
            if (!edict->IsFree() /*&& edict->m_fStateFlags & (FL_EDICT_CHANGED | FL_FULL_EDICT_CHANGED)*/ && !(edict->m_fStateFlags & FL_EDICT_DONTSEND)) {
                CBasePlayer *ent = reinterpret_cast<CBasePlayer *>(edict->GetUnknown());
                if (ent == nullptr || !(ent->GetFlags() & FL_FAKECLIENT) || ent->IsHLTV() || ent->IsReplay()) continue;
                bool isalive = ent->IsAlive();
                if (!isalive && ent->GetDeathTime() + 1.0f < gpGlobals->curtime) {
                    edict->m_fStateFlags &= ~(FL_EDICT_CHANGED | FL_FULL_EDICT_CHANGED);
                    edict->m_fStateFlags |= FL_EDICT_DONTSEND;
                }
                if ( (i + gpGlobals->tickcount) % 2 != 0) {
                    CBaseEntity *weapon = ent->GetActiveWeapon();
                    if (weapon != nullptr) {
                        weapon->edict()->m_fStateFlags &= ~(FL_EDICT_CHANGED | FL_FULL_EDICT_CHANGED);
                    }
                }
            }
        }
        firstTransmit = true;
        if (!IsParallel()) {
            DETOUR_STATIC_CALL(SV_ComputeClientPacks)(clientCount, clients, snapshot);

            if (firstPack && packinfoOffset != 0) {
                firstPack = false;
            }
            return;
        }

        // Synchronous compute first
        for (int clientIndex = 0; clientIndex < clientCount; clientIndex ++) {
            computedPackInfos[clientIndex] = false;
            checkTransmitComplete[clientIndex] = false;
            edict_t *edict = world_edict + clients[clientIndex]->m_nEntityIndex;
            if (edict->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION) {
                engine->BuildEntityClusterList(edict, &static_cast<CServerNetworkProperty *>(edict->GetNetworkable())->m_PVSInfo);
                edict->m_fStateFlags &= ~FL_EDICT_DIRTY_PVS_INFORMATION;
            }
        }
        int numareas = g_BSPData.GetRef().numareas;
        auto areas = g_BSPData.GetRef().map_areas;
        static std::vector<uint16_t> floodnums;
        if ((int)floodnums.size() < numareas * clientCount) {
            floodnums.resize(numareas * clientCount);
        }

        packWorkFinished = false;
        int packWorkTaskCount = (int) threadPoolPackWork.get_thread_count();
        CFrameSnapshotManager &snapmgr = g_FrameSnapshotManager;

        std::vector<edict_t *> *entitiesWithNotUpdatedChangedProps = new std::vector<edict_t *>[packWorkTaskCount];
        std::vector<edict_t *> *entitiesWithNotUpdatedFullyChangedProps = new std::vector<edict_t *>[packWorkTaskCount];

        // Threaded networking starts here
        threadPool.push_task([&](){
            for (int clientIndex = 0; clientIndex < clientCount; clientIndex ++) {
                auto client = clients[clientIndex];
                client->SetupPackInfo(snapshot);
                for (int i = 0; i < numareas; i++) {
                    floodnums[clientIndex * numareas + i] = (uint16_t) areas[i].floodnum;
                }
                computedPackInfos[clientIndex] = true;
                computedPackInfos[clientIndex].notify_all();
            }
            

            for (int i = 0; i < packWorkTaskCount; i++) {
                threadPoolPackWork.push_task([&](int num){
                    size_t workEntryCount = snapshot->m_nValidEntities/packWorkTaskCount+1;
                    PackWork_t *workEntities = (PackWork_t *) operator new[](workEntryCount * sizeof(PackWork_t));
                    size_t workEntitiesCount = 0;
                    for (int i = num; i < snapshot->m_nValidEntities; i += packWorkTaskCount) {
                        int idx = snapshot->m_pValidEntities[i];
                        auto edict = world_edict + idx;
                        // Don't update props for non networked entities
                        if ((edict->m_fStateFlags & FL_EDICT_DONTSEND) && ((CBaseEntity *)edict->GetUnknown())->FirstMoveChild() == nullptr 
                            && snapmgr.UsePreviouslySentPacket(snapshot, idx, edict->m_NetworkSerialNumber)) {

                            if (edict->m_fStateFlags & FL_FULL_EDICT_CHANGED)
                                entitiesWithNotUpdatedFullyChangedProps[num].push_back(edict);
                            else if (edict->m_fStateFlags & FL_EDICT_CHANGED)
                                entitiesWithNotUpdatedChangedProps[num].push_back(edict);
                            continue;
                        } 
                        PackWork_t &work = workEntities[workEntitiesCount++];
                        work.nIdx = idx;
                        work.pEdict = edict;
                        work.pSnapshot = snapshot;
                    }
                    PackWork(workEntities, workEntitiesCount);
                    delete[] workEntities;
                }, i);
            }
            threadPoolPackWork.wait_for_tasks();
            packWorkFinished = true;
            for (int i = 0; i < packWorkTaskCount && i < clientCount; i++) {
                threadPoolPackWork.push_task([&](int num){
                    for (int clientIndex = num; clientIndex < clientCount; clientIndex += packWorkTaskCount) {
                        while (!checkTransmitComplete[clientIndex]) checkTransmitComplete[clientIndex].wait(false);

                        auto client = clients[clientIndex];
                        if (client->m_bIsHLTV || client->m_bIsReplay) continue;
                        CClientFrame *pFrame = client->GetSendFrame();
                        if (pFrame) {
                            client->SendSnapshot(pFrame);
                            client->UpdateSendState();
                        }
                    }
                }, i);
            }
            packWorkFinished.notify_all();
        });

        for (int clientIndex = 0; clientIndex < clientCount; clientIndex++) {
            while (!computedPackInfos[clientIndex]) computedPackInfos[clientIndex].wait(false);

            auto client = clients[clientIndex];
            auto transmit = (CCheckTransmitInfo *)((uintptr_t)client + packinfoOffset);
            areasConnected = floodnums.data() + (clientIndex * numareas);
            serverGameEnts->CheckTransmit(transmit, snapshot->m_pValidEntities, snapshot->m_nValidEntities);
            areasConnected = nullptr;
            checkTransmitComplete[clientIndex] = true;
            checkTransmitComplete[clientIndex].notify_all();
        }

        packWorkFinished.wait(false);

        SCOPED_INCREMENT(rc_SV_ComputeClientPacks);
        DETOUR_STATIC_CALL(SV_ComputeClientPacks)(clientCount, clients, snapshot);

        for (int i = 0; i < clientCount; i++) {
            CGameClient *client = clients[i];
            if (!client->m_bIsHLTV && !client->m_bIsReplay) continue;
            CClientFrame *pFrame = client->GetSendFrame();
            if ( pFrame )
            {
                client->SendSnapshot( pFrame );
                client->UpdateSendState();
            }
        }
        for (int i = 0; i < snapshot->m_nValidEntities; i++) {
            int edictID = snapshot->m_pValidEntities[i];
            edict_t *edict = world_edict + edictID;
            edict->m_fStateFlags &= ~(FL_FULL_EDICT_CHANGED|FL_EDICT_CHANGED);
        }

        for (int i = 0; i < packWorkTaskCount; i++) {
            for (auto edict : entitiesWithNotUpdatedChangedProps[i]) {
                edict->m_fStateFlags |= FL_EDICT_CHANGED;
            }

            for (auto edict : entitiesWithNotUpdatedFullyChangedProps[i]) {
                edict->m_fStateFlags |= (FL_EDICT_CHANGED | FL_FULL_EDICT_CHANGED);
            }
        }

        delete[] entitiesWithNotUpdatedChangedProps;
        delete[] entitiesWithNotUpdatedFullyChangedProps;

        threadPool.wait_for_tasks();
        threadPoolPackWork.wait_for_tasks();

        for (int i = 0; i < clientCount; i++) {
            CGameClient *client = clients[i];
            clients[i] = nullptr;
        }
	}

    DETOUR_DECL_MEMBER(void, CParallelProcessor_CGameClient_Run, CGameClient **clients, long items, long maxthreads, void *pool)
	{
        if (IsParallel()) {
            return;
        }

        DETOUR_MEMBER_CALL(CParallelProcessor_CGameClient_Run)(clients, items, maxthreads, pool);
    }

#ifdef SE_IS_TF2
    DETOUR_DECL_MEMBER_CALL_CONVENTION(__gcc_regcall, void, CTFPlayer_AddObject, CBaseObject *object)
	{
        DETOUR_MEMBER_CALL(CTFPlayer_AddObject)(object);
        reinterpret_cast<CTFPlayer *>(this)->NetworkStateChanged();
    }

    DETOUR_DECL_MEMBER(void, CTFPlayer_RemoveObject, CBaseObject *object)
	{
        DETOUR_MEMBER_CALL(CTFPlayer_RemoveObject)(object);
        reinterpret_cast<CTFPlayer *>(this)->NetworkStateChanged();
    }

    DETOUR_DECL_MEMBER(void, CTFPlayerShared_AddCond, ETFCond nCond, float flDuration, CBaseEntity *pProvider)
	{
        auto shared = reinterpret_cast<CTFPlayerShared *>(this);
        if (pProvider != shared->GetConditionProvider(nCond))
            reinterpret_cast<CTFPlayer *>(shared->GetOuter())->NetworkStateChanged();
            
		DETOUR_MEMBER_CALL(CTFPlayerShared_AddCond)(nCond, flDuration, pProvider);
	}
#endif

    DETOUR_DECL_MEMBER(void , CAnimationLayer_StudioFrameAdvance, float flInterval, CBaseAnimating *pOwner)
	{
        
        int oldFlags = pOwner->edict()->m_fStateFlags;
        DETOUR_MEMBER_CALL(CAnimationLayer_StudioFrameAdvance)(flInterval, pOwner);
        if (pOwner->IsPlayer()) pOwner->edict()->m_fStateFlags = oldFlags | (pOwner->edict()->m_fStateFlags & ~(FL_FULL_EDICT_CHANGED));
    }

#ifdef SE_IS_TF2
    DETOUR_DECL_MEMBER_CALL_CONVENTION(__gcc_regcall,void , CBaseAnimatingOverlay_FastRemoveLayer, int layer)
#else
    DETOUR_DECL_MEMBER(void, CBaseAnimatingOverlay_FastRemoveLayer, int layer)
#endif
	{
        auto &flags = reinterpret_cast<CBaseAnimatingOverlay *>(this)->edict()->m_fStateFlags;
        int oldFlags = flags;
        DETOUR_MEMBER_CALL(CBaseAnimatingOverlay_FastRemoveLayer)(layer);
        if (reinterpret_cast<CBaseAnimatingOverlay *>(this)->IsPlayer()) flags = oldFlags | (flags & ~(FL_FULL_EDICT_CHANGED));
    }

    DETOUR_DECL_MEMBER(void , CBaseAnimatingOverlay_StudioFrameAdvance)
	{;
        auto &flags = reinterpret_cast<CBaseAnimatingOverlay *>(this)->edict()->m_fStateFlags;
        int oldFlags = flags;
        DETOUR_MEMBER_CALL(CBaseAnimatingOverlay_StudioFrameAdvance)();
        if (reinterpret_cast<CBaseAnimatingOverlay *>(this)->IsPlayer()) flags = oldFlags | (flags & ~(FL_FULL_EDICT_CHANGED));
    }

    DETOUR_DECL_MEMBER(void , CBaseAnimatingOverlay_SetLayerCycle, int layer, float cycle)
	{
        auto &flags = reinterpret_cast<CBaseAnimatingOverlay *>(this)->edict()->m_fStateFlags;
        int oldFlags = flags;
        DETOUR_MEMBER_CALL(CBaseAnimatingOverlay_SetLayerCycle)(layer, cycle);
        if (reinterpret_cast<CBaseAnimatingOverlay *>(this)->IsPlayer()) flags = oldFlags | (flags & ~(FL_FULL_EDICT_CHANGED));
    }

#ifdef SE_IS_TF2
    DETOUR_DECL_MEMBER(void , CMultiPlayerAnimState_AddToGestureSlot, int iGestureSlot, Activity iGestureActivity, bool bAutoKill)
	{
        auto &flags = reinterpret_cast<CMultiPlayerAnimState *>(this)->m_pPlayer->edict()->m_fStateFlags;
        int oldFlags = flags;
        DETOUR_MEMBER_CALL(CMultiPlayerAnimState_AddToGestureSlot)(iGestureSlot, iGestureActivity, bAutoKill);
        flags = oldFlags | (flags & ~(FL_FULL_EDICT_CHANGED));
    }

    DETOUR_DECL_MEMBER_CALL_CONVENTION(__gcc_regcall,void , CMultiPlayerAnimState_RestartGesture, int iGestureSlot, Activity iGestureActivity, bool bAutoKill)
	{
        auto &flags = reinterpret_cast<CMultiPlayerAnimState *>(this)->m_pPlayer->edict()->m_fStateFlags;
        int oldFlags = flags;
        DETOUR_MEMBER_CALL(CMultiPlayerAnimState_RestartGesture)(iGestureSlot, iGestureActivity, bAutoKill);
        flags = oldFlags | (flags & ~(FL_FULL_EDICT_CHANGED));
    }
#endif

    DETOUR_DECL_MEMBER(void, CBaseEntity_D2)
	{
        auto entity = reinterpret_cast<CBaseEntity *>(this);
        auto edict = entity->edict();
        if (edict != nullptr) {
            prop_write_offset[edict->m_EdictIndex].clear();
            prop_value_old[edict->m_EdictIndex].clear();
            entity_frame_bit_size[edict->m_EdictIndex] = 0;
        }
        DETOUR_MEMBER_CALL(CBaseEntity_D2)();
    }

    /*DETOUR_DECL_MEMBER(int, SendTable_WriteAllDeltaProps, int iTick, int *iOutProps, int nMaxOutProps)
	{
		int result = DETOUR_MEMBER_CALL(SendTable_WriteAllDeltaProps)(iTick, iOutProps, nMaxOutProps);
        if (result == -1)
            result = 0
        return result;
    }*/
    

    DETOUR_DECL_STATIC(int, SendTable_WriteAllDeltaProps, const SendTable *pTable,					
        const void *pFromData,
        const int	nFromDataBits,
        const void *pToData,
        const int nToDataBits,
        const int nObjectID,
        bf_write *pBufOut)
    {

        SCOPED_INCREMENT_IF(rc_SendTable_WriteAllDeltaProps, nObjectID != -1);
        //if (nObjectID != -1)
        //    DevMsg("F %s\n", pTable->GetName());
        return DETOUR_STATIC_CALL(SendTable_WriteAllDeltaProps)(pTable, pFromData, nFromDataBits, pToData, nToDataBits, nObjectID, pBufOut);
    }

    DETOUR_DECL_STATIC(IChangeFrameList*, AllocChangeFrameList, int nProperties, int iCurTick)
    {
        CachedChangeFrameList *pRet = new CachedChangeFrameList;
        pRet->Init( nProperties, iCurTick);
        return pRet;
    }
    
#ifdef SE_IS_TF2
    DETOUR_DECL_MEMBER(void, CPopulationManager_SetPopulationFilename, const char *filename)
    {
        DETOUR_MEMBER_CALL(CPopulationManager_SetPopulationFilename)(filename);
        // Original function uses MAKE_STRING to set file name, which is bad
        if (TFObjectiveResource() != nullptr) {
            TFObjectiveResource()->m_iszMvMPopfileName = AllocPooledString(filename);
        }
    }
#endif


    DETOUR_DECL_STATIC(void, InvalidateSharedEdictChangeInfo)
    {
        g_SharedEdictChangeInfo->m_iSerialNumber = 0;
        g_SharedEdictChangeInfo->m_nChangeInfos = 1;
        world_accessor->SetChangeInfoSerialNumber(0);
        world_accessor->SetChangeInfo(0);
    }

#ifdef SE_IS_TF2
    THINK_FUNC_DECL(SetVisibleStateDelay) {
		auto player = reinterpret_cast<CBasePlayer *>(this);
        if (player->IsAlive()) return;
        for (int i = 0; i < player->GetNumWearables(); i++) {
            CEconWearable *wearable = player->GetWearable(i);
            if (wearable == nullptr) continue;
            wearable->edict()->m_fStateFlags |= FL_EDICT_DONTSEND;
        }
        for (int i = 0; i < player->WeaponCount(); i++) {
            CBaseCombatWeapon *weapon = player->GetWeapon(i);
            if (weapon == nullptr) continue;
            weapon->edict()->m_fStateFlags |= FL_EDICT_DONTSEND;
        }
        for (int i = 0; i < MAX_VIEWMODELS; i++) {
            CBaseEntity *viewmodel = player->GetViewModel(i, false);
            if (viewmodel == nullptr) continue;
            viewmodel->edict()->m_fStateFlags |= FL_EDICT_DONTSEND;
        }
    }
    THINK_FUNC_DECL(SetVisibleStateWeapon) {
        auto weapon = reinterpret_cast<CBaseCombatWeapon *>(this);
        if (weapon->GetOwnerEntity() != nullptr && (!weapon->GetOwnerEntity()->IsAlive() || (weapon->GetOwnerEntity()->GetFlags() & FL_FAKECLIENT && weapon->m_iState != WEAPON_IS_ACTIVE))) {
            this->edict()->m_fStateFlags |= FL_EDICT_DONTSEND;
        }
    }
    THINK_FUNC_DECL(SetVisibleState) {
        if (this->GetOwnerEntity() != nullptr && !this->GetOwnerEntity()->IsAlive()) {
            this->edict()->m_fStateFlags |= FL_EDICT_DONTSEND;
        }
    }
    DETOUR_DECL_MEMBER(void, CBasePlayer_Event_Killed, const CTakeDamageInfo& info)
	{
		auto player = reinterpret_cast<CBasePlayer *>(this);
		DETOUR_MEMBER_CALL(CBasePlayer_Event_Killed)(info);
        THINK_FUNC_SET(player,SetVisibleStateDelay, gpGlobals->curtime + 0.5f);
	}
    
    DETOUR_DECL_MEMBER(void, CTFWeaponBase_OnActiveStateChanged, int oldState)
	{
		auto weapon = reinterpret_cast<CTFWeaponBase *>(this);
		DETOUR_MEMBER_CALL(CTFWeaponBase_OnActiveStateChanged)(oldState);
        int state = weapon->m_iState;
        if (state == WEAPON_IS_ACTIVE) {
            weapon->edict()->m_fStateFlags &= ~FL_EDICT_DONTSEND;
        }
        else if (weapon->GetOwnerEntity() != nullptr && weapon->GetOwnerEntity()->GetFlags() & FL_FAKECLIENT) {
            THINK_FUNC_SET(weapon,SetVisibleStateWeapon, gpGlobals->curtime + 0.5f);
        }
	}
#endif
    
    DETOUR_DECL_MEMBER(void, CBaseCombatWeapon_OnActiveStateChanged, int oldState)
	{
		auto weapon = reinterpret_cast<CBaseCombatWeapon *>(this);
		DETOUR_MEMBER_CALL(CBaseCombatWeapon_OnActiveStateChanged)(oldState);
        int state = weapon->m_iState;
        if (state == WEAPON_IS_ACTIVE) {
            weapon->edict()->m_fStateFlags &= ~FL_EDICT_DONTSEND;
            auto player = ToBasePlayer(weapon->GetOwnerEntity());
            if (player != nullptr) {
                
                CBaseEntity *viewmodel = player->GetViewModel(0, false);
                if (viewmodel != nullptr) {
                    viewmodel->edict()->m_fStateFlags &= ~FL_EDICT_DONTSEND;
                }
            }
        }
	}

    DETOUR_DECL_MEMBER(void, CBasePlayer_Spawn)
	{
		auto player = reinterpret_cast<CBasePlayer *>(this);
		DETOUR_MEMBER_CALL(CBasePlayer_Spawn)();
        player->edict()->m_fStateFlags &= ~FL_EDICT_DONTSEND;
#ifdef SE_IS_TF2
        for (int i = 0; i < player->GetNumWearables(); i++) {
            CEconWearable *wearable = player->GetWearable(i);
            if (wearable == nullptr) continue;
            wearable->edict()->m_fStateFlags &= ~FL_EDICT_DONTSEND;
        }
#endif
        for (int i = 0; i < player->WeaponCount(); i++) {
            CBaseCombatWeapon *weapon = player->GetWeapon(i);
            if (weapon == nullptr) continue;
            if (!(player->GetFlags() & FL_FAKECLIENT && weapon->m_iState != WEAPON_IS_ACTIVE))
            weapon->edict()->m_fStateFlags &= ~FL_EDICT_DONTSEND;
        }
        for (int i = 0; i < MAX_VIEWMODELS; i++) {
            CBaseEntity *viewmodel = player->GetViewModel(i, false);
            if (viewmodel == nullptr) continue;
            viewmodel->edict()->m_fStateFlags &= ~FL_EDICT_DONTSEND;
        }
	}
    
    static MemberFuncThunk<INetworkStringTableContainer *, void, int> ft_DirectUpdate("CNetworkStringTableContainer::DirectUpdate");

    class CNetworkStringTableContainer : public INetworkStringTableContainer
    {
    public:
        void DirectUpdate(int tickAck) { ft_DirectUpdate(this, tickAck); }
    
    private:
        
    };

    bool IsStandardPropProxy(SendVarProxyFn proxyFn) {
        
        static SendVarProxyFn standardProxies[] {
            gamedll->GetStandardSendProxies()->m_FloatToFloat,
            gamedll->GetStandardSendProxies()->m_Int16ToInt32,
            gamedll->GetStandardSendProxies()->m_Int32ToInt32,
            gamedll->GetStandardSendProxies()->m_Int8ToInt32,
            gamedll->GetStandardSendProxies()->m_UInt16ToInt32,
            gamedll->GetStandardSendProxies()->m_UInt32ToInt32,
            gamedll->GetStandardSendProxies()->m_UInt8ToInt32,
            gamedll->GetStandardSendProxies()->m_VectorToVector,
            DLLSendProxy_StringToString.GetRef(),
            DLLSendProxy_VectorXYToVectorXY.GetRef(),
            DLLSendProxy_QAngles.GetRef(),
            DLLSendProxy_Color32ToInt.GetRef(),
            DLLSendProxy_EHandleToInt.GetRef(),
            DLLSendProxy_IntAddOne.GetRef(),
            DLLSendProxy_ShortAddOne.GetRef(),
            DLLSendProxy_StringT_To_String.GetRef(),
            DLLSendProxy_AngleToFloat.GetRef(),
        };
        for (auto standardProxy : standardProxies) {
            if (standardProxy == proxyFn) {
                return true;
            }
        }
        return proxyFn == nullptr;
    }

    void SetThreadCount(int threadCount) {
        threadNum = threadCount;
        threadPoolPackWork.reset(Max(1,threadCount));
    }

	class CMod : public IMod, public IModCallbackListener
	{
	public:
		CMod() : IMod("Perf:SendProp_Optimize")
		{
            
            MOD_ADD_DETOUR_MEMBER(CHLTVClient_SendSnapshot,"CHLTVClient::SendSnapshot");
            MOD_ADD_DETOUR_MEMBER(CParallelProcessor_PackWork_t_Run,   "CParallelProcessor<PackWork_t>::Run");
            //MOD_ADD_DETOUR_STATIC(SendTable_CalcDelta,   "SendTable_CalcDelta");
            MOD_ADD_REPLACE_FUNC_MEMBER(CBaseEdict_GetChangeAccessor,   "CBaseEdict::GetChangeAccessor");
            MOD_ADD_DETOUR_MEMBER(CAnimationLayer_StudioFrameAdvance,"CAnimationLayer::StudioFrameAdvance");
#ifdef SE_IS_TF2
            MOD_ADD_DETOUR_MEMBER(CBaseAnimatingOverlay_FastRemoveLayer,"CBaseAnimatingOverlay::FastRemoveLayer [clone]");
#else
            MOD_ADD_DETOUR_MEMBER(CBaseAnimatingOverlay_FastRemoveLayer,"CBaseAnimatingOverlay::FastRemoveLayer");
#endif
            MOD_ADD_DETOUR_MEMBER(CBaseAnimatingOverlay_StudioFrameAdvance,"CBaseAnimatingOverlay::StudioFrameAdvance");
            MOD_ADD_DETOUR_MEMBER(CBaseAnimatingOverlay_SetLayerCycle,"CBaseAnimatingOverlay::SetLayerCycle");

#ifdef SE_IS_TF2
            MOD_ADD_DETOUR_MEMBER(CMultiPlayerAnimState_AddToGestureSlot,"CMultiPlayerAnimState::AddToGestureSlot");
            MOD_ADD_DETOUR_MEMBER(CMultiPlayerAnimState_RestartGesture,"CMultiPlayerAnimState::RestartGesture [clone]");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_AddObject,   "CTFPlayer::AddObject [clone]");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_RemoveObject,"CTFPlayer::RemoveObject");
			MOD_ADD_DETOUR_MEMBER_PRIORITY(CTFPlayerShared_AddCond,"CTFPlayerShared::AddCond", LOWEST);
            MOD_ADD_DETOUR_MEMBER(CPopulationManager_SetPopulationFilename,"CPopulationManager::SetPopulationFilename");
#endif
            MOD_ADD_DETOUR_STATIC(SendTable_WritePropList,   "SendTable_WritePropList");
            MOD_ADD_DETOUR_STATIC(AllocChangeFrameList,   "AllocChangeFrameList");
		    MOD_ADD_DETOUR_STATIC(SendTable_CullPropsFromProxies, "SendTable_CullPropsFromProxies");
            MOD_ADD_DETOUR_MEMBER(CBaseEntity_D2,"~CBaseEntity [D2]");

            MOD_ADD_REPLACE_FUNC_MEMBER(CBaseEntity_SetTransmit,"CBaseEntity::SetTransmit");
            MOD_ADD_DETOUR_MEMBER(CServerGameEnts_CheckTransmit,"CServerGameEnts::CheckTransmit");
            MOD_ADD_DETOUR_MEMBER(CVEngineServer_CheckAreasConnected,"CVEngineServer::CheckAreasConnected");
            MOD_ADD_DETOUR_MEMBER(CGameClient_SetupPackInfo,"CGameClient::SetupPackInfo");
			MOD_ADD_DETOUR_STATIC_PRIORITY(SV_ComputeClientPacks, "SV_ComputeClientPacks", LOWEST);
			MOD_ADD_DETOUR_MEMBER(CParallelProcessor_CGameClient_Run, "CParallelProcessor<CGameClient *>::Run");
            MOD_ADD_DETOUR_STATIC(InvalidateSharedEdictChangeInfo,   "InvalidateSharedEdictChangeInfo");

            // Reduce some time spend in networking for dead players and their items
#ifdef SE_IS_TF2
			MOD_ADD_DETOUR_MEMBER(CBasePlayer_Event_Killed, "CBasePlayer::Event_Killed");
			MOD_ADD_DETOUR_MEMBER(CTFWeaponBase_OnActiveStateChanged, "CTFWeaponBase::OnActiveStateChanged");
#endif
			MOD_ADD_DETOUR_MEMBER(CBaseCombatWeapon_OnActiveStateChanged, "CBaseCombatWeapon::OnActiveStateChanged");
			MOD_ADD_DETOUR_MEMBER(CBasePlayer_Spawn, "CBasePlayer::Spawn");
            
		}
        virtual void PreLoad() override
        {
            
            
			g_SharedEdictChangeInfo = engine->GetSharedEdictChangeInfo();

            // Find player class (has DT_BasePlayer as a baseclass table)
            for(ServerClass *serverclass = gamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext) {
                for (int i = 0; i < serverclass->m_pTable->GetNumProps(); i++) {
                    if (serverclass->m_pTable->GetProp(i)->GetDataTable() != nullptr && strcmp(serverclass->m_pTable->GetProp(i)->GetDataTable()->GetName(), "DT_BasePlayer") == 0 ) {
                        playerSendTable = serverclass->m_pTable;
                        playerServerClass = serverclass;
                    }
                }
            }
		}

        void AddOffsetToList(ServerClassCache &cache, int offset, int index, int element) {
            int size = cache.prop_offset_sendtable.size();
            for (int i = 0; i < size; i++) {
                if (cache.prop_offset_sendtable[i].offset == (unsigned short) offset) {
                    cache.prop_offset_sendtable[i].index2 = (unsigned short) index;
                    return;
                }
            }
            cache.prop_offset_sendtable.emplace_back();
            PropIndexData &data = cache.prop_offset_sendtable.back();
            data.offset = (unsigned short) offset;
            data.index1 = (unsigned short) index;
            data.element = (unsigned short) element;
        };

        void PropScan(int off, SendTable *s_table, int &index)
        {
            for (int i = 0; i < s_table->GetNumProps(); ++i) {
                SendProp *s_prop = s_table->GetProp(i);

                if (s_prop->GetDataTable() != nullptr) {
                    PropScan(off + s_prop->GetOffset(), s_prop->GetDataTable(), index);
                }
                else {
                    //Msg("Scan Data table for %d %s %d is %d %d %d\n", index, s_prop->GetName(),  off + s_prop->GetOffset(), off, s_prop->GetProxyFn(), s_prop->GetDataTableProxyFn());
                    index++;
                    //onfound(s_prop, off + s_prop->GetOffset());
                }
            }
        }

        void RecurseStack(ServerClassCache &cache, unsigned char* stack, CSendNode *node, CSendTablePrecalc *precalc)
        {
            //stack[node->m_RecursiveProxyIndex] = strcmp(node->m_pTable->GetName(), "DT_TFNonLocalPlayerExclusive") == 0;
            stack[node->m_RecursiveProxyIndex] = node->m_DataTableProxyIndex;
            if (node->m_DataTableProxyIndex < 254) {
                //cache.send_nodes[node->m_DataTableProxyIndex] = node;
                player_local_exclusive_send_proxy[node->m_DataTableProxyIndex] = precalc->m_DatatableProps[node->m_iDatatableProp]->GetDataTableProxyFn() == local_sendtable_proxy;
            }
            
            //("data %d %d %s %d\n", node->m_RecursiveProxyIndex, stack[node->m_RecursiveProxyIndex], node->m_pTable->GetName(), node->m_nRecursiveProps);
            for (int i = 0; i < node->m_Children.Count(); i++) {
                CSendNode *child = node->m_Children[i];
                RecurseStack(cache, stack, child, precalc);
            }
        }

        virtual bool OnLoad() {
// // Older GCC has trouble compiling it, easiest way is to just disable it
// #if !(defined(__GNUC__) && (__GNUC__ < 12))
//             Warning("Sigsegv-MvM must be compiled with at least GCC 12 to enable Sendprop Optimize");
//             return false;
// #endif
            sendproxies = gamedll->GetStandardSendProxies();
            datatable_sendtable_proxy = sendproxies->m_DataTableToDataTable;
            local_sendtable_proxy = sendproxies->m_SendLocalDataTable;
            
            player_local_exclusive_send_proxy = new bool[playerSendTable->m_pPrecalc->m_nDataTableProxies];
            for(ServerClass *serverclass = gamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext) {
                //DevMsg("Crash1\n");
                SendTable *sendTable = serverclass->m_pTable;
                auto serverClassCache = new ServerClassCache();
                if (sendTable == playerSendTable) {
                    player_class_cache = serverClassCache;
                }

                // Reuse unused variable
                sendTable->m_pPrecalc->m_pDTITable = serverClassCache;
                int propcount = sendTable->m_pPrecalc->m_Props.Count();
                //DevMsg("%s %d %d\n", pTable->GetName(), propcount, pTable->GetNumProps());
                
                CPropMapStack pmStack( sendTable->m_pPrecalc, sendproxies );
                serverClassCache->prop_offsets = new unsigned short[propcount];
                //serverClassCache.send_nodes = new CSendNode *[playerSendTable->m_pPrecalc->m_nDataTableProxies];
                pmStack.Init();

                //int reduce_coord_prop_offset = 0;

                //DevMsg("Crash2\n");
                int t = 0;
                PropScan(0,sendTable, t);
                unsigned char proxyStack[256];

                RecurseStack(*serverClassCache, proxyStack, &sendTable->m_pPrecalc->m_Root , sendTable->m_pPrecalc);
                serverClassCache->prop_cull = new unsigned char[sendTable->m_pPrecalc->m_Props.Count()];
                serverClassCache->prop_propproxy_first = new unsigned short[sendTable->m_pPrecalc->m_nDataTableProxies];
                for (int i = 0; i < sendTable->m_pPrecalc->m_nDataTableProxies; i++) {
                    serverClassCache->prop_propproxy_first[i] = INVALID_PROP_INDEX;
                }

                for (int iToProp = 0; iToProp < sendTable->m_pPrecalc->m_Props.Count(); iToProp++)
                { 
                    const SendProp *pProp = sendTable->m_pPrecalc->m_Props[iToProp];

                    pmStack.SeekToProp( iToProp );

                    
                    //player_local_exclusive_send_proxy[proxyStack[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]] = player_prop_cull[iToProp] < 254 && pProp->GetDataTableProxyFn() == sendproxies->m_SendLocalDataTable;
                    
                    //bool local2 = pProp->GetDataTableProxyFn() == sendproxies->m_SendLocalDataTable;
                // bool local = player_local_exclusive_send_proxy[player_prop_cull[iToProp]];
                    //Msg("Local %s %d %d %d %d\n",pProp->GetName(), local, local2, sendproxies->m_SendLocalDataTable, pProp->GetDataTableProxyFn());

                    auto dataTableIndex = proxyStack[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]];
                    serverClassCache->prop_cull[iToProp] = dataTableIndex;
                    if (dataTableIndex < sendTable->m_pPrecalc->m_nDataTableProxies) {
                        serverClassCache->prop_propproxy_first[dataTableIndex] = iToProp;
                    }
                    if ((int)pmStack.GetCurStructBase() != 0) {

                        int offset = pProp->GetOffset() + (int)pmStack.GetCurStructBase() - 1;
                        
                        int elementCount = 1;
                        int elementStride = 0;
                        int propIdToUse = iToProp;
                        int offsetToUse = offset;
                        auto arrayProp = pProp;
                        if ( pProp->GetType() == DPT_Array )
                        {
                            offset = pProp->GetArrayProp()->GetOffset() + (int)pmStack.GetCurStructBase() - 1;
                            elementCount = pProp->m_nElements;
                            elementStride = pProp->m_ElementStride;
                            pProp = pProp->GetArrayProp();
                            offsetToUse = (int)pmStack.GetCurStructBase() - 1;
                        }

                        serverClassCache->prop_offsets[propIdToUse] = offsetToUse;


                        //if (pProp->GetType() == DPT_Vector || pProp->GetType() == DPT_Vector )
                        //    propIndex |= PROP_INDEX_VECTOR_ELEM_MARKER;
                        
                        if (offset != 0/*IsStandardPropProxy(pProp->GetProxyFn())*/) {

                            if (offset != 0)
                            {
                                int offset_off = offset;
                                for ( int j = 0; j < elementCount; j++ )
                                {
                                    AddOffsetToList(*serverClassCache, offset_off, propIdToUse, j);
                                    if (pProp->GetType() == DPT_Vector) {
                                        AddOffsetToList(*serverClassCache, offset_off + 4, propIdToUse, j);
                                        AddOffsetToList(*serverClassCache, offset_off + 8, propIdToUse, j);
                                    }
                                    else if (pProp->GetType() == DPT_VectorXY) {
                                        AddOffsetToList(*serverClassCache, offset_off + 4, propIdToUse, j);
                                    }
                                    offset_off += elementStride;
                                }
                            }
                        }
                        else {
                            // if (sendTable == playerSendTable) {
                            //     Msg("prop %d %s %d\n", iToProp, pProp->GetName(), offset);
                            // }
                            serverClassCache->prop_special.push_back({propIdToUse});
                        }
                    }
                    else {
                        auto &datatableSpecial = serverClassCache->datatable_special[sendTable->m_pPrecalc->m_DatatableProps[pmStack.m_pIsPointerModifyingProxy[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]->m_iDatatableProp]];
                        
                        //serverClassCache->prop_special.push_back({iToProp, pmStack.m_iBaseOffset[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]], pProp, pmStack.m_pIsPointerModifyingProxy[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]});
                        datatableSpecial.propIndexes.push_back(iToProp);
                        datatableSpecial.baseOffset = pmStack.m_iBaseOffset[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]];

                        serverClassCache->prop_offsets[iToProp] = pProp->GetOffset();
                    }
                    //Msg("Data table for %d %s %d is %d %d %d %d\n", iToProp, pProp->GetName(),  pProp->GetOffset() + (int)pmStack.GetCurStructBase() - 1, (int)pmStack.GetCurStructBase(), pProp->GetProxyFn(), pProp->GetDataTableProxyFn(), pmStack.m_pIsPointerModifyingProxy[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]) ;

                    //int bitsread_pre = toBits.GetNumBitsRead();

                    /*if (pProp->GetFlags() & SPROP_COORD_MP)) {
                        if ((int)pmStack.GetCurStructBase() != 0) {
                            reduce_coord_prop_offset += toBits.GetNumBitsRead() - bitsread_pre;
                            player_prop_coord.push_back(iToProp);
                            Msg("bits: %d\n", toBits.GetNumBitsRead() - bitsread_pre);
                        }
                    }*/
                }
                
                //Msg("End\n");
                // if (FStrEq(serverclass->GetName(), "CTFGameRulesProxy")) {
                //     for (auto &prop : serverClassCache->prop_special) {
                //         Msg("Special prop %d \n", prop.index);
                //     }
                //     for (auto &[node, special] : serverClassCache->datatable_special) {
                //         Msg("Special table %s %d\n", node->m_pTable->GetName(), special.baseOffset);
                //         for (auto &propId : special.propIndexes) {
                //             Msg("prop in table %d %s\n", propId, sendTable->m_pPrecalc->m_Props[propId]->GetName());
                //         }
                //     }
                // }
                world_edict = INDEXENT(0);
                if (world_edict != nullptr){
                    world_accessor = static_cast<CVEngineServer *>(engine)->GetChangeAccessorStatic(world_edict);
                    world_change_info = &g_SharedEdictChangeInfo->m_ChangeInfos[0];
                }
            }
            
            

            // void *predictable_id_func = nullptr;
            // for (int i = 0; i < playerSendTable->m_pPrecalc->m_DatatableProps.Count(); i++) {
            //     const char *name  = playerSendTable->m_pPrecalc->m_DatatableProps[i]->GetName();
            //     if (strcmp(name, "predictable_id") == 0) {
            //         predictable_id_func = playerSendTable->m_pPrecalc->m_DatatableProps[i]->GetDataTableProxyFn();
            //     }
            // }

            //CDetour *detour = new CDetour("SendProxy_SendPredictableId", predictable_id_func, GET_STATIC_CALLBACK(SendProxy_SendPredictableId), GET_STATIC_INNERPTR(SendProxy_SendPredictableId));
            //this->AddDetour(detour);
            return true;//detour->Load();
        }

        virtual void OnUnload() override
		{
	        UnloadDetours();
            CFrameSnapshotManager &snapmgr = g_FrameSnapshotManager;
            /*edict_t * world_edict = INDEXENT(0);
                if (!world_edict[i].IsFree()) {
                    PackedEntity *pPrevFrame = snapmgr.GetPreviouslySentPacket( i, world_edict[i].m_NetworkSerialNumber);
                    if (pPrevFrame != nullptr && pPrevFrame->m_pChangeFrameList != nullptr){
                        pPrevFrame->m_pChangeFrameList->Release();
                        pPrevFrame->m_pChangeFrameList = AllocChangeFrameList(pPrevFrame->m_pServerClass->m_pTable->m_pPrecalc->m_Props.Count(), gpGlobals->tickcount);
                    }
                }
            }*/
            /*auto changelist = frame_first;
            while (changelist != nullptr) {
                auto changeframenew = AllocChangeFrameList(changelist->GetNumProps(), gpGlobals->tickcount);
                memcpy(changelist, changeframenew, sizeof(CChangeFrameList));

                changelist = changelist->next;
            }*/

            for (CFrameSnapshot *snap : snapmgr.m_FrameSnapshots) {
                for (int i = 0; i < snap->m_nNumEntities; i++) {
                    CFrameSnapshotEntry *entry = snap->m_pEntities + i;
                    if (entry != nullptr){
                        
                        PackedEntity *packedEntity = reinterpret_cast< PackedEntity * >(entry->m_pPackedData);
                        if (packedEntity != nullptr && packedEntity->m_pChangeFrameList != nullptr) {
                            
                            packedEntity->m_pChangeFrameList->Release();
                            packedEntity->m_pChangeFrameList = AllocChangeFrameList(packedEntity->m_pServerClass->m_pTable->m_pPrecalc->m_Props.Count(), gpGlobals->tickcount);
                        }
                    }
                }
            }
            for (int i = 0; i < MAX_EDICTS; i++) {
                PackedEntity *packedEntity = reinterpret_cast< PackedEntity * >(snapmgr.m_pPackedData[i]);
                if (packedEntity != nullptr && packedEntity->m_pChangeFrameList != nullptr) {
                            
                    packedEntity->m_pChangeFrameList->Release();
                    packedEntity->m_pChangeFrameList = AllocChangeFrameList(packedEntity->m_pServerClass->m_pTable->m_pPrecalc->m_Props.Count(), gpGlobals->tickcount);
                }
            }
            
		}

        virtual bool ShouldReceiveCallbacks() const override { return this->IsEnabled(); }
        
        virtual void OnEnable() override
		{
            ConVarRef sv_parallel_packentities("sv_parallel_packentities");
            ConVarRef sv_parallel_sendsnapshot("sv_parallel_sendsnapshot");
            ConVarRef sv_instancebaselines("sv_instancebaselines");
            sv_parallel_packentities.SetValue(true);
            sv_parallel_sendsnapshot.SetValue(false);
            //sv_instancebaselines.SetValue(false);
            ConVarRef sv_maxreplay("sv_maxreplay");
            if (sv_maxreplay.GetFloat() == 0.0f) {
                sv_maxreplay.SetValue(0.05f);
            }
            extern ConVar cvar_threads;
            SetThreadCount(cvar_threads.GetInt());
        }

        virtual void OnDisable() override
        {
            SetThreadCount(0);
        }

		virtual void LevelInitPostEntity() override
		{
            world_edict = INDEXENT(0);
            world_accessor = static_cast<CVEngineServer *>(engine)->GetChangeAccessorStatic(world_edict);
            world_change_info = &g_SharedEdictChangeInfo->m_ChangeInfos[0];
            
            ConVarRef sv_parallel_packentities("sv_parallel_packentities");
            ConVarRef sv_parallel_sendsnapshot("sv_parallel_sendsnapshot");
            ConVarRef sv_instancebaselines("sv_instancebaselines");
            sv_parallel_packentities.SetValue(true);
            sv_parallel_sendsnapshot.SetValue(false);
            //sv_instancebaselines.SetValue(false);
            ConVarRef sv_maxreplay("sv_maxreplay");
            if (sv_maxreplay.GetFloat() == 0.0f) {
                sv_maxreplay.SetValue(0.05f);
            }
		}

	};
	CMod s_Mod;
	
	
	ConVar cvar_enable("sig_perf_sendprop_optimize", "0", FCVAR_NOTIFY,
		"Mod: improve sendprop encoding performance by preventing full updates on clients",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			s_Mod.Toggle(static_cast<ConVar *>(pConVar)->GetBool());
		});

    ConVar cvar_threads("sig_network_threads", "3", FCVAR_NONE, "Additional threads used for networking", true, 0, true, 10, 
    [](IConVar *pConVar, const char *pOldValue, float flOldValue){
        if (s_Mod.IsEnabled()) {
            SetThreadCount(cvar_threads.GetInt());
        }
    });
}
