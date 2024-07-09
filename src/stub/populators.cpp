#include "stub/populators.h"
#include "mem/extract.h"

#if defined _LINUX

static constexpr uint8_t s_Buf_CPopulationManager_m_RespecPoints[] = {
	0x55,                               // +0000  push ebp
	0x89, 0xe5,                         // +0001  mov ebp,esp
	0x53,                               // +0003  push ebx
	0x83, 0xec, 0x3c,                   // +0004  sub esp,0x3c
	0x8b, 0x5d, 0x08,                   // +0007  mov ebx,[ebp+this]
	0x8d, 0x83, 0xe4, 0x06, 0x00, 0x00, // +000A  lea eax,[ebx+0xVVVVVVVV]
	0x50,                               // +0010  push eax
	0xe8, 0xe6, 0x64, 0x00, 0x00,       // +0011  call CUtlRBTree<...>::RemoveAll
};

struct CExtract_CPopulationManager_m_RespecPoints : public IExtract<CPopulationManager::SteamIDMap *>
{
	using T = CPopulationManager::SteamIDMap *;
	
	CExtract_CPopulationManager_m_RespecPoints() : IExtract<T>(sizeof(s_Buf_CPopulationManager_m_RespecPoints)) {}
	
	virtual bool GetExtractInfo(ByteBuf& buf, ByteBuf& mask) const override
	{
		buf.CopyFrom(s_Buf_CPopulationManager_m_RespecPoints);
		
		mask.SetRange(0x04 + 2, 1, 0x00);
		mask.SetRange(0x0a + 2, 4, 0x00);
		mask.SetRange(0x11 + 1, 4, 0x00);
		
		return true;
	}
	
	virtual const char *GetFuncName() const override   { return "CPopulationManager::ResetRespecPoints"; }
	virtual uint32_t GetFuncOffMin() const override    { return 0x0000; }
	virtual uint32_t GetFuncOffMax() const override    { return 0x0000; }
	virtual uint32_t GetExtractOffset() const override { return 0x000a + 2; }
	virtual T AdjustValue(T val) const override        { return reinterpret_cast<T>((uintptr_t)val); }
};


static constexpr uint8_t s_Buf_CPopulationManager_m_bAllocatedBots[] = {
	0x8b, 0x75, 0x08,                         // +0000  mov esi,[ebp+this]
	0x80, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, // +0003  cmp byte ptr [esi+0xVVVVVVVV],0
	0x74, 0x00,                               // +000A  jz +0x??
};

struct CExtract_CPopulationManager_m_bAllocatedBots : public IExtract<bool *>
{
	using T = bool *;
	
	CExtract_CPopulationManager_m_bAllocatedBots() : IExtract<T>(sizeof(s_Buf_CPopulationManager_m_bAllocatedBots)) {}
	
	virtual bool GetExtractInfo(ByteBuf& buf, ByteBuf& mask) const override
	{
		buf.CopyFrom(s_Buf_CPopulationManager_m_bAllocatedBots);
		
		mask.SetRange(0x03 + 2, 2, 0x00);
		mask.SetRange(0x0a + 1, 1, 0x00);
		
		return true;
	}
	
	virtual const char *GetFuncName() const override   { return "CPopulationManager::AllocateBots"; }
	virtual uint32_t GetFuncOffMin() const override    { return 0x0000; }
	virtual uint32_t GetFuncOffMax() const override    { return 0x0010; } // @ +0x0006
	virtual uint32_t GetExtractOffset() const override { return 0x0003 + 2; }
};

static constexpr uint8_t s_Buf_CPopulationManager_m_pTemplates[] = {
	0x8B, 0x83, 0x90, 0x05, 0x00, 0x00,
	0x85, 0xC0,                            
};

struct CExtract_CPopulationManager_m_pTemplates : public IExtract<KeyValues **>
{
	using T = KeyValues **;
	
	CExtract_CPopulationManager_m_pTemplates() : IExtract<T>(sizeof(s_Buf_CPopulationManager_m_pTemplates)) {}
	
	virtual bool GetExtractInfo(ByteBuf& buf, ByteBuf& mask) const override
	{
		buf.CopyFrom(s_Buf_CPopulationManager_m_pTemplates);
		
		mask.SetRange(0x00 + 2, 4, 0x00);
		
		return true;
	}
	
	virtual const char *GetFuncName() const override   { return "CPopulationManager::~CPopulationManager [D2]"; }
	virtual uint32_t GetFuncOffMin() const override    { return 0x0000; }
	virtual uint32_t GetFuncOffMax() const override    { return 0x00FF; }
	virtual uint32_t GetExtractOffset() const override { return 0x0000 + 2; }
};

/*static constexpr uint8_t s_Buf_CWave_m_waveSpawnVector[] = {
	0x55,                    // 0000
	0x89, 0xE5,              // 0001
	0x56,                    // 0003
	0x8B, 0x45, 0x08,        // 0004
	0x53,                    // 0007
	0x8B, 0x58, 0x00,        // 0008
	0x85, 0xDB               // 000B
};

struct CExtract_CWave_m_waveSpawnVector : public IExtract<CUtlVector< CWaveSpawnPopulator * > *>
{
	using T = CUtlVector< CWaveSpawnPopulator * > *;
	
	CExtract_CPopulationManager_m_bAllocatedBots() : IExtract<T>(sizeof(s_Buf_CWave_m_waveSpawnVector)) {}
	
	virtual bool GetExtractInfo(ByteBuf& buf, ByteBuf& mask) const override
	{
		buf.CopyFrom(s_Buf_CWave_m_waveSpawnVector);
		
		mask.SetRange(0x08 + 2, 1, 0x00);
		
		return true;
	}
	
	virtual const char *GetFuncName() const override   { return "CWave::IsDoneWithNonSupportWaves"; }
	virtual uint32_t GetFuncOffMin() const override    { return 0x0000; }
	virtual uint32_t GetFuncOffMax() const override    { return 0x0000; } // @ +0x0006
	virtual uint32_t GetExtractOffset() const override { return 0x0008 + 2; }
	virtual T AdjustValue(T val) const override        { return reinterpret_cast<T>((uintptr_t)val - 0x4); }
};*/

#elif defined _WINDOWS

using CExtract_CPopulationManager_m_RespecPoints   = IExtractStub;
using CExtract_CPopulationManager_m_bAllocatedBots = IExtractStub;
using CExtract_CPopulationManager_m_pTemplates = IExtractStub;

#endif


MemberFuncThunk<CPopulationManager *, bool, KeyValues *> CPopulationManager::ft_LoadMvMMission("CPopulationManager::LoadMvMMission");
MemberFuncThunk<CPopulationManager *, CWave *>           CPopulationManager::ft_GetCurrentWave("CPopulationManager::GetCurrentWave");
MemberFuncThunk<CPopulationManager *, void>              CPopulationManager::ft_ResetMap      ("CPopulationManager::ResetMap");
MemberFuncThunk<CPopulationManager *, void>              CPopulationManager::ft_PauseSpawning ("CPopulationManager::PauseSpawning");
MemberFuncThunk<CPopulationManager *, void>              CPopulationManager::ft_UnpauseSpawning("CPopulationManager::UnpauseSpawning");
MemberFuncThunk<CPopulationManager *, void>              CPopulationManager::ft_AllocateBots   ("CPopulationManager::AllocateBots");
MemberFuncThunk<CPopulationManager *, bool>              CPopulationManager::ft_IsInEndlessWaves("CPopulationManager::IsInEndlessWaves");
MemberFuncThunk<CPopulationManager *, void, CTFPlayer *> CPopulationManager::ft_RemovePlayerAndItemUpgradesFromHistory      ("CPopulationManager::RemovePlayerAndItemUpgradesFromHistory");
MemberFuncThunk<CPopulationManager *, CUtlVector< CUpgradeInfo > *, CTFPlayer *>              CPopulationManager::ft_GetPlayerUpgradeHistory ("CPopulationManager::GetPlayerUpgradeHistory");
MemberFuncThunk<CPopulationManager *, void, const char*> CPopulationManager::ft_SetPopulationFilename("CPopulationManager::SetPopulationFilename");
MemberFuncThunk<CPopulationManager *, int, CTFPlayer *>  CPopulationManager::ft_GetPlayerCurrencySpent("CPopulationManager::GetPlayerCurrencySpent");
MemberFuncThunk<CPopulationManager *, void, int, float>  CPopulationManager::ft_JumpToWave("CPopulationManager::JumpToWave");
MemberFuncThunk<CPopulationManager *, PlayerUpgradeHistory *, CSteamID> CPopulationManager::ft_FindOrAddPlayerUpgradeHistory("CPopulationManager::FindOrAddPlayerUpgradeHistory");
MemberFuncThunk<CPopulationManager *, void, int>         CPopulationManager::ft_SetCheckpoint("CPopulationManager::SetCheckpoint");

StaticFuncThunk<int, CUtlVector<CTFPlayer *> *> CPopulationManager::ft_CollectMvMBots("CPopulationManager::CollectMvMBots");
StaticFuncThunk<void, CUtlVector<CUtlString> &> CPopulationManager::ft_FindDefaultPopulationFileShortNames("CPopulationManager::FindDefaultPopulationFileShortNames");

IMPL_EXTRACT(CPopulationManager::SteamIDMap, CPopulationManager, m_RespecPoints,   new CExtract_CPopulationManager_m_RespecPoints());
IMPL_EXTRACT(bool,                           CPopulationManager, m_bAllocatedBots, new CExtract_CPopulationManager_m_bAllocatedBots());
IMPL_RELATIVE(KeyValues *,                   CPopulationManager, m_pTemplates,     m_bAllocatedBots, -sizeof(bool) - sizeof(uintptr_t));
IMPL_RELATIVE(int,                           CPopulationManager, m_nStartingCurrency, m_pTemplates, +12);


GlobalThunk<CPopulationManager *> g_pPopulationManager("g_pPopulationManager");


MemberFuncThunk<CWave *, void, string_t, int, unsigned int> CWave::ft_AddClassType      ("CWave::AddClassType");
MemberFuncThunk<CWave *, void>                              CWave::ft_ForceFinish       ("CWave::ForceFinish");
MemberFuncThunk<CWave *, void>                              CWave::ft_ActiveWaveUpdate  ("CWave::ActiveWaveUpdate");
MemberFuncThunk<CWave *, void>                              CWave::ft_WaveCompleteUpdate("CWave::WaveCompleteUpdate");
MemberFuncThunk<CWave *, bool>                              CWave::ft_IsDoneWithNonSupportWaves("CWave::IsDoneWithNonSupportWaves");

MemberFuncThunk<CWaveSpawnPopulator *, int> CWaveSpawnPopulator::ft_GetCurrencyAmountPerDeath("CWaveSpawnPopulator::GetCurrencyAmountPerDeath");
MemberFuncThunk<CWaveSpawnPopulator *, void, CWaveSpawnPopulator::InternalStateType> CWaveSpawnPopulator::ft_SetState("CWaveSpawnPopulator::SetState");

MemberFuncThunk<CMissionPopulator *, bool, int> CMissionPopulator::ft_UpdateMission("CMissionPopulator::UpdateMission");


MemberVFuncThunk<IPopulationSpawner *, string_t, int                   > IPopulationSpawner::vt_GetClassIcon(TypeName<CTankSpawner>(), "CTankSpawner::GetClassIcon");
MemberVFuncThunk<IPopulationSpawner *, bool, int                       > IPopulationSpawner::vt_IsMiniBoss  (TypeName<CTankSpawner>(), "CTankSpawner::IsMiniBoss");
MemberVFuncThunk<IPopulationSpawner *, bool, CTFBot::AttributeType, int> IPopulationSpawner::vt_HasAttribute(TypeName<CSquadSpawner>(), "CSquadSpawner::HasAttribute");
MemberVFuncThunk<IPopulationSpawner *, bool, KeyValues *               > IPopulationSpawner::vt_Parse       (TypeName<CWaveSpawnPopulator>(), "CWaveSpawnPopulator::Parse");

StaticFuncThunk<IPopulationSpawner *, IPopulator *, KeyValues *> IPopulationSpawner::ft_ParseSpawner("IPopulationSpawner::ParseSpawner");


StaticFuncThunk<bool, const Vector&> ft_IsSpaceToSpawnHere("IsSpaceToSpawnHere");
StaticFuncThunk<bool, CTFBot::EventChangeAttributes_t &, KeyValues *> ft_ParseDynamicAttributes("ParseDynamicAttributes");
StaticFuncThunk<int, int, bool> ft_MannVsMachineStats_GetAcquiredCredits("MannVsMachineStats_GetAcquiredCredits");
StaticFuncThunk<void, EventInfo *, const char *> ft_FireEvent("FireEvent");
StaticFuncThunk<EventInfo *, KeyValues *> ft_ParseEvent("ParseEvent");

GlobalThunkRW<int> CWaveSpawnPopulator::m_reservedPlayerSlotCount("CWaveSpawnPopulator::m_reservedPlayerSlotCount");