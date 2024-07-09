#include "mod.h"
#include "util/scope.h"
#include "stub/server.h"
#include <sys/resource.h>


namespace Mod::Prof::Frame_Prof
{
    CCycleCount timespent;
    CCycleCount timespent2;
    CCycleCount timespent2Start;
    CCycleCount timespent2End;
    float prevTime = 0.0f;
    float prevTime2 = 0.0f;
	int bytesPrevReliable = 0;
	int bytesPrev = 0;
	int bytesPrevVoice = 0;
	bool prevOverflow = false;
	int maxsize = 0;
	int maxsizereliable= 0;
	int maxsizevoice= 0;
	DETOUR_DECL_STATIC(void, _Host_RunFrame, float dt)
	{
        CTimeAdder timer(&timespent);
		DETOUR_STATIC_CALL(_Host_RunFrame)(dt);
        timer.End();
        if (floor(gpGlobals->curtime/5) != floor(prevTime) ) {
            Msg("Frame time: %f\n", timespent.GetSeconds()/5);
            timespent.Init();
            prevTime = gpGlobals->curtime/5;
        }
		//Msg("ones: %d %d %d %d\n", bytesPrev, bytesPrevReliable, bytesPrevVoice, prevOverflow);
		bytesPrevReliable = 0;
		bytesPrev = 0;
		bytesPrevVoice = 0;
		prevOverflow = false;
	}
	
	DETOUR_DECL_STATIC(void, Host_CheckDumpMemoryStats)
	{
		DETOUR_STATIC_CALL(Host_CheckDumpMemoryStats)();
		timespent2End.Sample();
		timespent2.m_Int64 += timespent2End.m_Int64 - timespent2Start.m_Int64;
		static struct rusage s_lastUsage;
		struct rusage currentUsage;
        if (floor(gpGlobals->curtime/5) != floor(prevTime2) ) {
			float cpu_usage = 1;
			if ( getrusage( RUSAGE_SELF, &currentUsage ) == 0 )
			{
				double flTimeDiff = (currentUsage.ru_utime.tv_sec + (currentUsage.ru_utime.tv_usec / 1000000.0)) - (s_lastUsage.ru_utime.tv_sec + (s_lastUsage.ru_utime.tv_usec / 1000000.0)) + 
						(currentUsage.ru_stime.tv_sec + (currentUsage.ru_stime.tv_usec / 1000000.0)) - (s_lastUsage.ru_stime.tv_sec + (s_lastUsage.ru_stime.tv_usec / 1000000.0));

				s_lastUsage = currentUsage;
				cpu_usage = flTimeDiff;
			}
            Msg("Frame time: %fs CPU usage: %.1f%\n", timespent2.GetSeconds()/5, cpu_usage/5*100);
            timespent2.Init();
            prevTime2 = gpGlobals->curtime/5;
        }
	}

	

	DETOUR_DECL_MEMBER(void, CMapReslistGenerator_RunFrame)
	{
		timespent2Start.Sample();
		DETOUR_MEMBER_CALL(CMapReslistGenerator_RunFrame)();
	}

	DETOUR_DECL_MEMBER(int, CNetChan_SendDatagram, bf_write *bf)
	{
		auto chan = reinterpret_cast<INetChannel *>(this);
		bytesPrev = MAX(bytesPrev ,chan->GetNumBitsWritten(false) + (bf != nullptr ? bf->GetNumBitsWritten() : 0));
		bytesPrevReliable = MAX(bytesPrevReliable, chan->GetNumBitsWritten(true));
		prevOverflow |= chan->IsOverflowed();
		auto result = DETOUR_MEMBER_CALL(CNetChan_SendDatagram)(bf);
		return result;
	}

    DETOUR_DECL_MEMBER(bool, CGameClient_ShouldSendMessages)
	{
		auto client = reinterpret_cast<CGameClient *>(this);
		auto result = DETOUR_MEMBER_CALL(CGameClient_ShouldSendMessages)();
		if (!result && client->m_bFakePlayer && !client->m_bIsHLTV && !client->m_bIsReplay) {
        	static ConVarRef sv_stressbots("sv_stressbots");
			return sv_stressbots.GetBool();
		}
		return result;
	}

    DETOUR_DECL_MEMBER(void, CGameServer_SendClientMessages, bool sendSnapshots)
	{
		DETOUR_MEMBER_CALL(CGameServer_SendClientMessages)(sendSnapshots);
        static ConVarRef sv_stressbots("sv_stressbots");
        if (sv_stressbots.GetBool()) {
            for (int i = 0; i < sv->GetClientCount(); i++) {
                auto client = static_cast<CGameClient *>(sv->GetClient(i));
                if (client->IsActive() && client->m_bFakePlayer && !client->m_bIsHLTV && !client->m_bIsReplay) {
                    CClientFrame *pFrame = client->GetSendFrame();
                    if ( pFrame )
                    {
                        client->m_pLastSnapshot = pFrame->GetSnapshot(); 
                        client->UpdateAcknowledgedFramecount( pFrame->tick_count );
                        client->m_nForceWaitForTick = -1;
                        client->m_nDeltaTick = pFrame->tick_count;
                    }
                }
            }
        }
	}

	
	class CMod : public IMod
	{
	public:
		CMod() : IMod("Prof:Frame_Prof")
		{
			//MOD_ADD_DETOUR_STATIC(_Host_RunFrame,               "_Host_RunFrame");
			MOD_ADD_DETOUR_STATIC(Host_CheckDumpMemoryStats,               "Host_CheckDumpMemoryStats");
			MOD_ADD_DETOUR_MEMBER(CMapReslistGenerator_RunFrame,               "CMapReslistGenerator::RunFrame");
			//MOD_ADD_DETOUR_MEMBER(CNetChan_SendDatagram,               "CNetChan::SendDatagram");

            // sv_stressbots: properly send snapshots to fake clients
			MOD_ADD_DETOUR_MEMBER(CGameServer_SendClientMessages, "CGameServer::SendClientMessages");
			MOD_ADD_DETOUR_MEMBER(CGameClient_ShouldSendMessages, "CGameClient::ShouldSendMessages");
		}
	};
	CMod s_Mod;
	
	
	ConVar cvar_enable("sig_prof_frame_prof", "0", FCVAR_NOTIFY,
		"Mod: show frame time %",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			s_Mod.Toggle(static_cast<ConVar *>(pConVar)->GetBool());
		});
}