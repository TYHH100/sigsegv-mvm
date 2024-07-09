#include "mod.h"
#include "stub/tfbot.h"
#include "util/scope.h"
#include "util/clientmsg.h"
#include "util/misc.h"
#include "util/value_override.h"
#include "stub/strings.h"
#include "stub/tfplayer.h"
#include "stub/gamerules.h"
#include "stub/misc.h"
#include "stub/server.h"
#include "stub/team.h"
#include "stub/objects.h"
#include "stub/tfweaponbase.h"
#include "stub/tf_player_resource.h"
#include "stub/lagcompensation.h"
#include "stub/particles.h"
#include "mod/pop/popmgr_extensions.h"
#include "mod/etc/mapentity_additions.h"
#include "mod/mvm/player_limit.h"
#include "mod/common/text_hud.h"
#include "util/iterate.h"
#include <steam/steam_gameserver.h>
#include <steam/isteamgameserver.h>
#include <gamemovement.h>
#include <worldsize.h>

class SVC_ServerInfo
{
public:
    uintptr_t vtable;
    uintptr_t pad;
	bool				m_bReliable;	// true if message should be send reliable
	INetChannel			*m_NetChannel;	// netchannel this message is from/for
    IServerMessageHandler *m_pMessageHandler;
    int			m_nProtocol;	// protocol version
	int			m_nServerCount;	// number of changelevels since server start
	bool		m_bIsDedicated;  // dedicated server ?	
	bool		m_bIsHLTV;		// HLTV server ?
	bool		m_bIsReplay;	// Replay server ?
	char		m_cOS;			// L = linux, W = Win32
	CRC32_t		m_nMapCRC;		// server map CRC (only used by older demos)
	MD5Value_t	m_nMapMD5;		// server map MD5
	int			m_nMaxClients;	// max number of clients on server
};

namespace Mod::Etc::Extra_Player_Slots
{

#ifdef SE_IS_TF2
    constexpr int DEFAULT_MAX_PLAYERS = 101;
// Fake to clients that the players in extra slots are CBaseCombatCharacter to prevent forced disconnect
#define FAKE_PLAYER_CLASS
#elif definded(SE_IS_CSS)
    constexpr int DEFAULT_MAX_PLAYERS = 65;
#endif
    inline bool ExtraSlotsEnabled();
    
    extern ConVar cvar_enable;

    ConVar sig_etc_extra_player_slots_count("sig_etc_extra_player_slots_count", "101", FCVAR_NOTIFY,
		"Extra player slot count. Requires map restart to function");

    ConVar sig_etc_extra_player_slots_allow_bots("sig_etc_extra_player_slots_allow_bots", "0", FCVAR_NOTIFY | FCVAR_GAMEDLL,
		"Allow bots to use extra player slots",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			if (sig_etc_extra_player_slots_allow_bots.GetBool() && cvar_enable.GetBool() && flOldValue == 0.0f && ExtraSlotsEnabled() && gpGlobals->maxClients < sig_etc_extra_player_slots_count.GetInt()) {
                engine->ChangeLevel(STRING(gpGlobals->mapname), nullptr);
            }
		});

    ConVar sig_etc_extra_player_slots_allow_players("sig_etc_extra_player_slots_allow_players", "0", FCVAR_NOTIFY,
		"Allow players to use extra player slots",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			if (sig_etc_extra_player_slots_allow_players.GetBool() && cvar_enable.GetBool() && flOldValue == 0.0f && ExtraSlotsEnabled() && gpGlobals->maxClients < sig_etc_extra_player_slots_count.GetInt()) {
                engine->ChangeLevel(STRING(gpGlobals->mapname), nullptr);
            }
		});

    ConVar sig_etc_extra_player_slots_voice_display_fix("sig_etc_extra_player_slots_voice_display_fix", "0", FCVAR_NOTIFY,
		"Fixes voice chat indicator showing with more than 64 slots");

    ConVar sig_etc_extra_player_slots_no_death_cam("sig_etc_extra_player_slots_no_death_cam", "1", FCVAR_NOTIFY | FCVAR_GAMEDLL,
		"Does not display death cam when killed by players in extra slots");

    ConVar sig_etc_extra_player_slots_show_player_names("sig_etc_extra_player_slots_show_player_names", "0", FCVAR_NOTIFY,
		"Show player names in extra player slots when mouse over");
    
    
    RefCount rc_CBaseServer_CreateFakeClient_HLTV;
    RefCount rc_CBaseServer_CreateFakeClient;

    inline bool ExtraSlotsEnabled()
    {
        return sig_etc_extra_player_slots_count.GetInt() > DEFAULT_MAX_PLAYERS;
    }

    bool ExtraSlotsEnabledForBots()
    {
        return ExtraSlotsEnabled() && cvar_enable.GetBool() && sig_etc_extra_player_slots_allow_bots.GetBool();
    }

    bool ExtraSlotsEnabledExternal()
    {
        return ExtraSlotsEnabled() && cvar_enable.GetBool();
    }

    bool MapHasExtraSlots(const char *map) 
	{
        if (!ExtraSlotsEnabled()) return false;
		if (sig_etc_extra_player_slots_allow_bots.GetBool() || sig_etc_extra_player_slots_allow_players.GetBool()) return true;

		FileFindHandle_t missionHandle;
		char poppathfind[256];
		snprintf(poppathfind, sizeof(poppathfind), "scripts/population/%s_*.pop", map);
		for (const char *missionName = filesystem->FindFirstEx(poppathfind, "GAME", &missionHandle);
						missionName != nullptr; missionName = filesystem->FindNext(missionHandle)) {
			
			char poppath[256];
			snprintf(poppath, sizeof(poppath), "%s%s","scripts/population/", missionName);
			KeyValues *kv = new KeyValues("kv");
			kv->UsesConditionals(false);
			if (kv->LoadFromFile(filesystem, poppath)) {
				FOR_EACH_SUBKEY(kv, subkey) {

					if (FStrEq(subkey->GetName(), "AllowBotExtraSlots") && subkey->GetBool() ) {
                        
		                filesystem->FindClose(missionHandle);
                        kv->deleteThis();
						return true;
					}
				}
			}
            kv->deleteThis();
		}
		filesystem->FindClose(missionHandle);
    
        snprintf(poppathfind, sizeof(poppathfind), "scripts/population/%s.pop", map);
        KeyValues *kv = new KeyValues("kv");
        kv->UsesConditionals(false);
        if (kv->LoadFromFile(filesystem, poppathfind)) {
            FOR_EACH_SUBKEY(kv, subkey) {

                if (FStrEq(subkey->GetName(), "AllowBotExtraSlots") && subkey->GetBool() ) {
                    kv->deleteThis();
                    return true;
                }
            }
        }
        kv->deleteThis();

        return false;
	}

    DETOUR_DECL_MEMBER(CBaseClient *, CBaseServer_CreateFakeClient, const char *name)
	{
        static ConVarRef tv_name("tv_name");
        static int counter = 0;
        SCOPED_INCREMENT(rc_CBaseServer_CreateFakeClient);
        SCOPED_INCREMENT_IF(rc_CBaseServer_CreateFakeClient_HLTV, tv_name.GetString() == name);
        if (tv_name.GetString() != name && strcmp(name, "TFBot") == 0) {
            name = STRING(AllocPooledString(CFmtStr("TFBot %d", ++counter)));
        }

        //ForEachEntity([](CBaseEntity *ent){
         //   Msg("Entity %d %s\n", ent->entindex(), ent->GetClassname());
		//	});
		return DETOUR_MEMBER_CALL(CBaseServer_CreateFakeClient)(name);
	}

    inline int GetHLTVSlot()
    {
        return DEFAULT_MAX_PLAYERS - 1;
    }

    int force_create_at_slot = -1;
    void SetForceCreateAtSlot(int slot) {
        force_create_at_slot = slot;
    }

    bool DebugForcePlayersUseExtraSlots()
    {
        return false;
    }

    DETOUR_DECL_MEMBER(CBaseClient *, CBaseServer_GetFreeClient, netadr_t &adr)
	{
        auto server = reinterpret_cast<CBaseServer *>(this);
        if (server == hltv || ((!ExtraSlotsEnabled() || gpGlobals->maxClients < DEFAULT_MAX_PLAYERS + 1) && force_create_at_slot == -1)) return DETOUR_MEMBER_CALL(CBaseServer_GetFreeClient)(adr);

        if (DebugForcePlayersUseExtraSlots() || rc_CBaseServer_CreateFakeClient || force_create_at_slot != -1) {
			static ConVarRef tv_enable("tv_enable");
            std::vector<CBaseClient *> clientList;

            // Make sure all slots are taken
            if (server->GetClientCount() != server->GetMaxClients()) {
                // Set clients as fake so they are considered taken
                for (int i = 0; i < server->GetClientCount(); i++) {
                    CBaseClient *client = static_cast<CBaseClient *>(server->GetClient(i));
                    if (!client->IsConnected() && !client->IsFakeClient()) {
                        clientList.push_back(client);
                        client->m_bFakePlayer = true;
                    }
                }

                // Create clients to fill all slots
                while (server->GetClientCount() != server->GetMaxClients()) {
                    CBaseClient *lastClient = DETOUR_MEMBER_CALL(CBaseServer_GetFreeClient)(adr);
                    if (lastClient != nullptr) {
                        clientList.push_back(lastClient);
                        lastClient->m_bFakePlayer = true;
                    }
                }
                for (auto client : clientList) {
                    client->m_bFakePlayer = false;
                }
            }
            
            if (force_create_at_slot != -1) {
                CBaseClient *client = static_cast<CBaseClient *>(server->GetClient(force_create_at_slot));
                force_create_at_slot = -1;
                return !client->IsConnected() && !client->IsFakeClient() ? client : nullptr;
            }

            int desiredSlot = GetHLTVSlot();
            if (!rc_CBaseServer_CreateFakeClient_HLTV) {
                if (!sig_etc_extra_player_slots_allow_bots.GetBool()) {
                    desiredSlot = DEFAULT_MAX_PLAYERS - 1;
                }
                else {
#ifdef NO_MVM
                    desiredSlot = gpGlobals->maxClients - 1;
#else
                    if (TFGameRules()->IsMannVsMachineMode()) {
                        int red, blu, spectators, robots;
                        desiredSlot = Clamp(Mod::MvM::Player_Limit::GetSlotCounts(red, blu, spectators, robots) - 1, DEFAULT_MAX_PLAYERS - 1, server->GetMaxClients() - 1);
                    }
                    else {
                        desiredSlot = gpGlobals->maxClients - 1;
                    }
#endif
                }
            }

            for (int i = desiredSlot; i >= 0; i--) {
                CBaseClient *client = static_cast<CBaseClient *>(server->GetClient(i));
                if (!client->IsConnected() 
                && !client->IsFakeClient() 
                && !(i == GetHLTVSlot() && !rc_CBaseServer_CreateFakeClient_HLTV && tv_enable.GetBool())) {
                    return client;
                }
            }

            return nullptr;
        }
		auto client = DETOUR_MEMBER_CALL(CBaseServer_GetFreeClient)(adr);
        if (client != nullptr) {
            if ( !((sig_etc_extra_player_slots_allow_bots.GetBool() && rc_CBaseServer_CreateFakeClient) || (sig_etc_extra_player_slots_allow_players.GetBool() && !rc_CBaseServer_CreateFakeClient)) && !rc_CBaseServer_CreateFakeClient_HLTV && client->GetPlayerSlot() > DEFAULT_MAX_PLAYERS - 1) {
                return nullptr;
            }
            if (!rc_CBaseServer_CreateFakeClient_HLTV && client->GetPlayerSlot() == gpGlobals->maxClients - 1) {
                return nullptr;
            }
        }
        return client;
	}

    StaticFuncThunk<void, int> ft_SetupMaxPlayers("SetupMaxPlayers");
    //StaticFuncThunk<bool, bool, const char *, const char *> ft_Host_Changelevel("Host_Changelevel");
    //MemberFuncThunk<CBaseServer *, bool, const char *, const char *, const char *>              ft_SpawnServer("CGameServer::SpawnServer");

	DETOUR_DECL_MEMBER(void, CServerGameClients_GetPlayerLimits, int &minplayers, int &maxplayers, int &defaultplayers)
	{
		DETOUR_MEMBER_CALL(CServerGameClients_GetPlayerLimits)(minplayers,maxplayers,defaultplayers);
        if (ExtraSlotsEnabled())
		    maxplayers = sig_etc_extra_player_slots_count.GetInt();
	}

    edict_t *world_edict = nullptr;
    std::vector<CLagCompensationManager *> lag_compensation_copies;

	DETOUR_DECL_MEMBER(void, CLagCompensationManager_FrameUpdatePostEntityThink)
	{
        int preMaxPlayers = gpGlobals->maxClients;

        if (preMaxPlayers > DEFAULT_MAX_PLAYERS) {
            // Run lag compensation using multiple copies of compensation managers. Each manager manages MAX_PLAYERS players. For each manager copy extra MAX_PLAYERS slots into the first MAX_PLAYERS edict slots
            edict_t originalEdicts[DEFAULT_MAX_PLAYERS];
            memcpy(originalEdicts, world_edict+1, sizeof(edict_t) * DEFAULT_MAX_PLAYERS);

            for (int i = 1; i < (preMaxPlayers + DEFAULT_MAX_PLAYERS - 1) / DEFAULT_MAX_PLAYERS; i++) {
                int slotsCopy = Min(preMaxPlayers - i * DEFAULT_MAX_PLAYERS, DEFAULT_MAX_PLAYERS);
                memcpy(world_edict+1, world_edict+1 + DEFAULT_MAX_PLAYERS * i, sizeof(edict_t) * slotsCopy);
                gpGlobals->maxClients = slotsCopy;
                (reinterpret_cast<Detour_CLagCompensationManager_FrameUpdatePostEntityThink *>(lag_compensation_copies[i - 1])->*Actual)();
            }
            memcpy(world_edict+1, originalEdicts, sizeof(edict_t) * DEFAULT_MAX_PLAYERS);
            gpGlobals->maxClients = DEFAULT_MAX_PLAYERS;
        }
		DETOUR_MEMBER_CALL(CLagCompensationManager_FrameUpdatePostEntityThink)();
        gpGlobals->maxClients = preMaxPlayers;
	}
	
    int player_move_entindex = 0;
	DETOUR_DECL_MEMBER(void, CLagCompensationManager_StartLagCompensation, CBasePlayer *player, CUserCmd *cmd)
	{
		int preMaxPlayers = gpGlobals->maxClients;
        if (preMaxPlayers > DEFAULT_MAX_PLAYERS) {
            if (!player->IsRealPlayer() || player->GetObserverMode() != OBS_MODE_NONE) {
                for (int i = 1; i < (gpGlobals->maxClients + DEFAULT_MAX_PLAYERS - 1) / DEFAULT_MAX_PLAYERS; i++) {
                    (reinterpret_cast<Detour_CLagCompensationManager_StartLagCompensation *>(lag_compensation_copies[i - 1])->*Actual)(player, cmd);
                }
                DETOUR_MEMBER_CALL(CLagCompensationManager_StartLagCompensation)(player, cmd);
                return;
            }
            edict_t originalEdicts[DEFAULT_MAX_PLAYERS];
            memcpy(originalEdicts, world_edict+1, sizeof(edict_t) * DEFAULT_MAX_PLAYERS);

            for (int i = 1; i < (preMaxPlayers + DEFAULT_MAX_PLAYERS - 1) / DEFAULT_MAX_PLAYERS; i++) {
                int slotsCopy = Min(preMaxPlayers - i * DEFAULT_MAX_PLAYERS, DEFAULT_MAX_PLAYERS);
                memcpy(world_edict+1, world_edict+1 + DEFAULT_MAX_PLAYERS * i, sizeof(edict_t) * slotsCopy);
                for (int j = 1; j <= slotsCopy; j++) {
                    (world_edict+j)->m_EdictIndex = j;
                }
                gpGlobals->maxClients = slotsCopy;
                player_move_entindex = -i * DEFAULT_MAX_PLAYERS;
                (reinterpret_cast<Detour_CLagCompensationManager_StartLagCompensation *>(lag_compensation_copies[i - 1])->*Actual)(player, cmd);
            }
            player_move_entindex = 0;
            memcpy(world_edict+1, originalEdicts, sizeof(edict_t) * DEFAULT_MAX_PLAYERS);
            gpGlobals->maxClients = DEFAULT_MAX_PLAYERS;
        }
		DETOUR_MEMBER_CALL(CLagCompensationManager_StartLagCompensation)(player, cmd);
        gpGlobals->maxClients = preMaxPlayers;
	}
	
    DETOUR_DECL_MEMBER(void, CLagCompensationManager_BacktrackPlayer, CBasePlayer *player, float flTargetTime)
	{
        player->NetworkProp()->GetProp()->m_EdictIndex+=player_move_entindex;
		DETOUR_MEMBER_CALL(CLagCompensationManager_BacktrackPlayer)(player, flTargetTime);
        player->NetworkProp()->GetProp()->m_EdictIndex-=player_move_entindex;
    }

	DETOUR_DECL_MEMBER(void, CLagCompensationManager_FinishLagCompensation, CBasePlayer *player)
	{
		int preMaxPlayers = gpGlobals->maxClients;

        if (preMaxPlayers > DEFAULT_MAX_PLAYERS) {
            edict_t originalEdicts[DEFAULT_MAX_PLAYERS];
            memcpy(originalEdicts, world_edict+1, sizeof(edict_t) * DEFAULT_MAX_PLAYERS);

            for (int i = 1; i < (preMaxPlayers + DEFAULT_MAX_PLAYERS - 1) / DEFAULT_MAX_PLAYERS; i++) {
                int slotsCopy = Min(preMaxPlayers - i * DEFAULT_MAX_PLAYERS, DEFAULT_MAX_PLAYERS);
                memcpy(world_edict+1, world_edict+1 + DEFAULT_MAX_PLAYERS * i, sizeof(edict_t) * slotsCopy);
                gpGlobals->maxClients = slotsCopy;
                (reinterpret_cast<Detour_CLagCompensationManager_FinishLagCompensation *>(lag_compensation_copies[i - 1])->*Actual)(player);
            }
            memcpy(world_edict+1, originalEdicts, sizeof(edict_t) * DEFAULT_MAX_PLAYERS);
            gpGlobals->maxClients = DEFAULT_MAX_PLAYERS;
        }
		DETOUR_MEMBER_CALL(CLagCompensationManager_FinishLagCompensation)(player);
        gpGlobals->maxClients = preMaxPlayers;
	}

    int playerListOffset = 0;
	DETOUR_DECL_STATIC(void, SendProxy_PlayerList, const void *pProp, const void *pStruct, const void *pData, void *pOut, int iElement, int objectID)
	{
        if (!ExtraSlotsEnabled()) return DETOUR_STATIC_CALL(SendProxy_PlayerList)(pProp, pStruct, pData, pOut, iElement, objectID);
        if (iElement == 0) {
            playerListOffset = 0;
        }
        auto team = (CTeam *)(pData);
        int numplayers = team->GetNumPlayers();
        while (iElement + playerListOffset < numplayers - 1 && ENTINDEX(team->GetPlayer(iElement + playerListOffset)) > DEFAULT_MAX_PLAYERS) {
            playerListOffset += 1;
        }
        //Msg("Element %d offset %d\n", iElement, playerListOffset);
        return DETOUR_STATIC_CALL(SendProxy_PlayerList)(pProp, pStruct, pData, pOut, iElement + playerListOffset, objectID);
    }
    DETOUR_DECL_STATIC(int, SendProxyArrayLength_PlayerArray, const void *pStruct, int objectID)
	{
		int count = DETOUR_STATIC_CALL(SendProxyArrayLength_PlayerArray)(pStruct, objectID);
        int countpre = count;
        if (ExtraSlotsEnabled()) {
            auto team = (CTeam *)(pStruct);
            
            //Msg("Send proxy Team is %d %d %d\n", team->GetTeamNumber(), count, team->GetNumPlayers());

            for (int i = 0; i < countpre; i++) {
                //Msg("Player %d\n", team->GetPlayer(i));
                if (ENTINDEX(team->GetPlayer(i)) > DEFAULT_MAX_PLAYERS) {
                    count--;
                }
                //Msg("Player post\n");
            }
        }
        //Msg("Count %d Pre %d\n", count, countpre);
        return count;
    }

	DETOUR_DECL_MEMBER(void, CTeam_AddPlayer, CBasePlayer *player)
	{
        auto team = reinterpret_cast<CTFTeam *>(this);
        //Msg("Add plaayer Team is %d %d\n", team->GetTeamNumber(), player);
        if (ENTINDEX(player) < DEFAULT_MAX_PLAYERS + 1) {
            int insertindex = team->m_aPlayers->Count();
            while (insertindex > 0 && ENTINDEX(team->m_aPlayers[insertindex - 1]) > DEFAULT_MAX_PLAYERS) {
                insertindex--;
            }
            team->m_aPlayers->InsertBefore(insertindex, player);
            team->NetworkStateChanged();
            return;
        }
        DETOUR_MEMBER_CALL(CTeam_AddPlayer)(player);

        // team->m_aPlayers->Sort([](CBasePlayer * const *l, CBasePlayer * const *r){
        //     return ENTINDEX(*l) - ENTINDEX(*r);
        // });
        //for (auto player : team->m_aPlayers) {
        //    Msg("team post %d\n", ENTINDEX(player));
        //}
    }
    DETOUR_DECL_MEMBER(void, CTFTeam_SetTeamLeader, CBasePlayer *player)
	{
        auto team = reinterpret_cast<CTFTeam *>(this);
        //if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) {
        //    team->m_hLeader = nullptr;
        //    return;
        //}
        DETOUR_MEMBER_CALL(CTFTeam_SetTeamLeader)(player);
    }

	DETOUR_DECL_MEMBER(void, CTeam_RemovePlayer, CBasePlayer *player)
	{
        auto team = reinterpret_cast<CTFTeam *>(this);
        //Msg("Remove player Team is %d %d\n", team->GetTeamNumber(), player);
        //if (team->m_hLeader == player) {
        //    team->m_hLeader = nullptr;
        //}
        DETOUR_MEMBER_CALL(CTeam_RemovePlayer)(player);
    }

    DETOUR_DECL_MEMBER(void, CTFPlayer_UpdateOnRemove)
	{
        auto player = reinterpret_cast<CTFPlayer *>(this);
        DETOUR_MEMBER_CALL(CTFPlayer_UpdateOnRemove)();
        //Msg("Delete player %d %d\n", player->GetTeamNumber(), player);
    }

	DETOUR_DECL_MEMBER_CALL_CONVENTION(__gcc_regcall, int, CGameMovement_GetPointContentsCached, const Vector &point, int slot)
	{
        int oldIndex = -1;
        auto movement = reinterpret_cast<CGameMovement *>(this);
        if (movement->player != nullptr && ENTINDEX(movement->player) > DEFAULT_MAX_PLAYERS) {
            oldIndex = ENTINDEX(movement->player);
            movement->player->edict()->m_EdictIndex = DEFAULT_MAX_PLAYERS - 1;
            //return enginetrace->GetPointContents(point);
        }
        auto ret = DETOUR_MEMBER_CALL(CGameMovement_GetPointContentsCached)(point, slot);
        if (oldIndex != -1) {
            movement->player->edict()->m_EdictIndex = oldIndex;
        }
        return ret;
    }

	DETOUR_DECL_MEMBER(int, CGameMovement_CheckStuck)
	{
        auto movement = reinterpret_cast<CGameMovement *>(this);
        int oldIndex = -1;
        if (movement->player != nullptr && ENTINDEX(movement->player) > DEFAULT_MAX_PLAYERS) {
            oldIndex = ENTINDEX(movement->player);
            movement->player->edict()->m_EdictIndex = 0;
        }
        auto ret = DETOUR_MEMBER_CALL(CGameMovement_CheckStuck)();
        if (oldIndex != -1) {
            movement->player->edict()->m_EdictIndex = oldIndex;
        }
        return ret;
    }

	DETOUR_DECL_MEMBER(void, CTFGameStats_ResetPlayerStats, CTFPlayer* player) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_ResetPlayerStats)(player);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_ResetKillHistory, CTFPlayer* player) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_ResetKillHistory)(player);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_IncrementStat, CTFPlayer* player, int statType, int value) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_IncrementStat)(player, statType, value);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_SendStatsToPlayer, CTFPlayer* player, bool isAlive) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_SendStatsToPlayer)(player, isAlive);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_AccumulateAndResetPerLifeStats, CTFPlayer* player) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_AccumulateAndResetPerLifeStats)(player);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_Event_PlayerConnected, CTFPlayer* player) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_Event_PlayerConnected)(player);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_Event_PlayerDisconnectedTF, CTFPlayer* player) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_Event_PlayerDisconnectedTF)(player);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_Event_PlayerLeachedHealth, CTFPlayer* player, bool dispenser, float amount) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_Event_PlayerLeachedHealth)(player, dispenser, amount);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_TrackKillStats, CTFPlayer* attacker, CTFPlayer* victim) { if (ENTINDEX(attacker) > DEFAULT_MAX_PLAYERS || ENTINDEX(victim) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_TrackKillStats)(attacker, victim);}
    DETOUR_DECL_MEMBER(void *, CTFGameStats_FindPlayerStats, CTFPlayer* player) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return nullptr; return DETOUR_MEMBER_CALL(CTFGameStats_FindPlayerStats)(player);}
    DETOUR_DECL_MEMBER(void, CTFGameStats_Event_PlayerEarnedKillStreak, CTFPlayer* player) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFGameStats_Event_PlayerEarnedKillStreak)(player);}

    DETOUR_DECL_MEMBER(bool, CTFPlayerShared_IsPlayerDominated, int index) { if (index > DEFAULT_MAX_PLAYERS) return false; return DETOUR_MEMBER_CALL(CTFPlayerShared_IsPlayerDominated)(index);}
    DETOUR_DECL_MEMBER(bool, CTFPlayerShared_IsPlayerDominatingMe, int index) { if (index > DEFAULT_MAX_PLAYERS) return false; return DETOUR_MEMBER_CALL(CTFPlayerShared_IsPlayerDominatingMe)(index);}
    DETOUR_DECL_MEMBER(void, CTFPlayerShared_SetPlayerDominated, CTFPlayer * player, bool dominated) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFPlayerShared_SetPlayerDominated)(player, dominated);}
    DETOUR_DECL_MEMBER(void, CTFPlayerShared_SetPlayerDominatingMe, CTFPlayer * player, bool dominated) { if (ENTINDEX(player) > DEFAULT_MAX_PLAYERS) return; DETOUR_MEMBER_CALL(CTFPlayerShared_SetPlayerDominatingMe)(player, dominated);}
    

    DETOUR_DECL_MEMBER(void, CTFPlayerResource_SetPlayerClassWhenKilled, int iIndex, int iClass )
	{
        if (iIndex > DEFAULT_MAX_PLAYERS) {
            return;
        } 
        DETOUR_MEMBER_CALL(CTFPlayerResource_SetPlayerClassWhenKilled)(iIndex, iClass);
    }

    DETOUR_DECL_MEMBER(void, CBasePlayer_UpdatePlayerSound)
	{
        auto player = reinterpret_cast<CBasePlayer *>(this);
        if (player->entindex() > 96) return;
        DETOUR_MEMBER_CALL(CBasePlayer_UpdatePlayerSound)();
    }

    DETOUR_DECL_MEMBER(void, CSoundEnt_Initialize)
	{
        int oldMaxClients = gpGlobals->maxClients;
        if (gpGlobals->maxClients > 96) {
            gpGlobals->maxClients = 96;
        }
        DETOUR_MEMBER_CALL(CSoundEnt_Initialize)();
        gpGlobals->maxClients = oldMaxClients;
    }

    constexpr int VOICE_MAX_PLAYERS = ((DEFAULT_MAX_PLAYERS + 31) / 32) * 32; 
    GlobalThunk<CBitVec<VOICE_MAX_PLAYERS>[DEFAULT_MAX_PLAYERS]>  g_SentGameRulesMasks("g_SentGameRulesMasks");
    DETOUR_DECL_MEMBER(void, CVoiceGameMgr_ClientConnected, edict_t *edict)
	{
        if (ENTINDEX(edict) > DEFAULT_MAX_PLAYERS) return;
        
        DETOUR_MEMBER_CALL(CVoiceGameMgr_ClientConnected)(edict);
        g_SentGameRulesMasks.GetRef()[ENTINDEX(edict) - 1].SetAll();
    }

    DETOUR_DECL_MEMBER(void, CHLTVDirector_BuildActivePlayerList)
	{
        int oldMaxClients = gpGlobals->maxClients;
        if (gpGlobals->maxClients > DEFAULT_MAX_PLAYERS) {
            gpGlobals->maxClients = DEFAULT_MAX_PLAYERS;
        }
        DETOUR_MEMBER_CALL(CHLTVDirector_BuildActivePlayerList)();
        gpGlobals->maxClients = oldMaxClients;
    }

    DETOUR_DECL_MEMBER(void, CTFPlayer_HandleCommand_JoinTeam, const char * team)
	{
        char *ready = (char *)(TFGameRules()->m_bPlayerReady.Get());
        CTFPlayer *player = reinterpret_cast<CTFPlayer *>(this);
        char oldReady = ready[player->entindex()];
        Msg("pre ready %d \n", ready[player->entindex()]);
        DETOUR_MEMBER_CALL(CTFPlayer_HandleCommand_JoinTeam)(team);
        if (player->entindex() > DEFAULT_MAX_PLAYERS) {
            Msg("post ready %d\n", ready[player->entindex()]);
            ready[player->entindex()] = oldReady;
        }
    }

    DETOUR_DECL_MEMBER(void, CTFGameRules_PlayerReadyStatus_UpdatePlayerState, CTFPlayer *pTFPlayer, bool bState)
	{
        if (ENTINDEX(pTFPlayer) > DEFAULT_MAX_PLAYERS ) return;

        DETOUR_MEMBER_CALL(CTFGameRules_PlayerReadyStatus_UpdatePlayerState)(pTFPlayer, bState);
    }

    DETOUR_DECL_MEMBER(void, CTFGCServerSystem_ClientDisconnected, CSteamID steamid)
	{
        int *state = (int *)(TFGameRules()->m_ePlayerWantsRematch.Get());
        int oldstate = -1;
        int index = ENTINDEX(UTIL_PlayerBySteamID(steamid));
        if (index > DEFAULT_MAX_PLAYERS) {
            oldstate = state[index];
        }
        DETOUR_MEMBER_CALL(CTFGCServerSystem_ClientDisconnected)(steamid);
        if (oldstate != -1) {
            state[index] = oldstate;
        }
    }

    RefCount rc_CTFGameRules_ClientDisconnected;
    edict_t *disconnected_player_edict = nullptr;
    DETOUR_DECL_MEMBER(void, CTFGameRules_ClientDisconnected, edict_t *edict)
	{
		SCOPED_INCREMENT(rc_CTFGameRules_ClientDisconnected);
        disconnected_player_edict = edict;
		DETOUR_MEMBER_CALL(CTFGameRules_ClientDisconnected)(edict);
        disconnected_player_edict = nullptr;
	}

    DETOUR_DECL_MEMBER(int, CTFGameRules_GetTeamAssignmentOverride, CTFPlayer *pPlayer, int iWantedTeam, bool b1)
	{
		if (rc_CTFGameRules_ClientDisconnected && disconnected_player_edict != nullptr && iWantedTeam == 0 && pPlayer->entindex() == ENTINDEX(disconnected_player_edict)) {
            return 0;
        }
		return DETOUR_MEMBER_CALL(CTFGameRules_GetTeamAssignmentOverride)(pPlayer, iWantedTeam, b1);
	}

    DETOUR_DECL_MEMBER(bool, CTFGameMovement_CheckWater)
	{
		return false;//DETOUR_MEMBER_CALL(CTFGameMovement_CheckWater)();
	}

    CON_COMMAND(sig_get_entites, "")
    {
        ForEachEntity([](CBaseEntity *ent){
            Msg("Entity %d %s\n", ent->entindex(), ent->GetClassname());
            ent->PostClientActive();
			});
    }

    CON_COMMAND(sig_get_commands, "")
    {
        const ConCommandBase *cmd = icvar->GetCommands();
        for ( ; cmd; cmd = cmd->GetNext() )
        {
            Msg("%s\n", cmd->GetName());
        }
    }

    CON_COMMAND(sig_list_team, "")
    {
        for (int i = 0; i < 4; i++) {
            auto team = TFTeamMgr()->GetTeam(i);
            Msg("Team %d:\n", i);
            if (team != nullptr) {
                for (int j = 0; j < team->GetNumPlayers(); j++) {
                    Msg(" player %d\n", team->GetPlayer(j));
                }
            }
        }
    }

    DETOUR_DECL_MEMBER(ConVar *, CCvar_FindVar, const char *name)
	{
        Msg("CVar %s\n", name);
        return DETOUR_MEMBER_CALL(CCvar_FindVar)(name);
    }

    DETOUR_DECL_MEMBER(const char *, ConVar_GetName)
	{
        auto ret = DETOUR_MEMBER_CALL(ConVar_GetName)();
        
        Msg("CVarn %s\n", ret);
        return ret;
    }
    DETOUR_DECL_MEMBER(const char *, ConCommandBase_GetName)
	{
        auto ret = DETOUR_MEMBER_CALL(ConCommandBase_GetName)();
        
        Msg("CCmd %s\n", ret);
        return ret;
    }

    float talk_time[VOICE_MAX_PLAYERS];
    float talk_start_time[VOICE_MAX_PLAYERS];
    
    
    class CVoiceGameMgr {};
    MemberFuncThunk<CVoiceGameMgr *, void>  ft_CVoiceGameMgr_UpdateMasks("CVoiceGameMgr::UpdateMasks");
    StaticFuncThunk<bool, IClient *, int, char*, int64>  ft_SV_BroadcastVoiceData("SV_BroadcastVoiceData");

    void *voicemgr = nullptr;

    DETOUR_DECL_STATIC(bool, SV_BroadcastVoiceData, IClient * pClient, int nBytes, char *data, int64 xuid)
    {
        if (gpGlobals->maxClients > VOICE_MAX_PLAYERS && pClient->GetPlayerSlot() < VOICE_MAX_PLAYERS) {
            
            // Force update player voice masks
            bool update = talk_time[pClient->GetPlayerSlot()] < gpGlobals->curtime - 5.0f;
            talk_time[pClient->GetPlayerSlot()] = gpGlobals->curtime;
            if (update && voicemgr != nullptr) {
                talk_start_time[pClient->GetPlayerSlot()] = gpGlobals->curtime;
                ft_CVoiceGameMgr_UpdateMasks((CVoiceGameMgr *)voicemgr);

            }
        }    
        return DETOUR_STATIC_CALL(SV_BroadcastVoiceData)(pClient, nBytes, data, xuid);
    }

    CValueOverride_ConVar<bool> sv_alltalk("sv_alltalk");
    DETOUR_DECL_MEMBER(bool, CVoiceGameMgrHelper_CanPlayerHearPlayer, CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity)
	{
        if (gpGlobals->maxClients > VOICE_MAX_PLAYERS && sig_etc_extra_player_slots_voice_display_fix.GetBool()) {

            if (ENTINDEX(pTalker) > VOICE_MAX_PLAYERS)
                return false;
            
            if ((gpGlobals->maxClients > (VOICE_MAX_PLAYERS * 2 - 1) || ENTINDEX(pTalker) % VOICE_MAX_PLAYERS < gpGlobals->maxClients % VOICE_MAX_PLAYERS) && talk_time[ENTINDEX(pTalker) - 1] + 5.0f < gpGlobals->curtime)
                 return false;
        }
        if (sv_alltalk.GetOriginalValue()) {
            return true;
        }
        return DETOUR_MEMBER_CALL(CVoiceGameMgrHelper_CanPlayerHearPlayer)(pListener, pTalker, bProximity);
    }

    DETOUR_DECL_MEMBER(void, CVoiceGameMgr_UpdateMasks)
	{
        voicemgr = this;
        if (gpGlobals->maxClients > VOICE_MAX_PLAYERS && sig_etc_extra_player_slots_voice_display_fix.GetBool() && sv_alltalk.Get()) {
            sv_alltalk.Set(false);
        }

        DETOUR_MEMBER_CALL(CVoiceGameMgr_UpdateMasks)();
        sv_alltalk.Reset();
        // if (restoreAllTalk) {
        //     sv_alltalk.SetValue(true);
        // }
    }

    DETOUR_DECL_MEMBER(void, CTeamplayRoundBasedRules_State_Enter_PREROUND)
	{
        // Prematurely send bots to spawn. This way you can avoid edict limit being hit during map reload
        if (TFGameRules()->IsMannVsMachineMode()) {
            ForEachTFBot([](CTFBot *bot){
                if (bot->GetTeamNumber() > TEAM_SPECTATOR) {
                    bot->ChangeTeam(TEAM_SPECTATOR, false, true, false);
                }
            });
        }
        DETOUR_MEMBER_CALL(CTeamplayRoundBasedRules_State_Enter_PREROUND)();
    }

    DETOUR_DECL_MEMBER(void, CTFGameRules_PowerupModeKillCountCompare)
	{
        Msg("called kill count compare\n");
        DETOUR_MEMBER_CALL(CTFGameRules_PowerupModeKillCountCompare)();
    }

    DETOUR_DECL_MEMBER(void, CTFGameRules_PowerupModeInitKillCountTimer)
	{
        Msg("called init kill count timer\n");
        DETOUR_MEMBER_CALL(CTFGameRules_PowerupModeInitKillCountTimer)();
    }

    struct KillEvent
    {
        float time;
        IGameEvent *event;
        bool send = false;
        std::string names[3];
        std::string oldNames[3];
        int teams[3] = {-1, -1, -1};
        int usedPlayerIndexes[3] = {-1, -1, -1};
        
        std::vector<CBasePlayer *> participants;
        bool prepared = false;
    };

    std::deque<KillEvent> kill_events;

    bool CanUseThisPlayerForChangeName(int i, CBasePlayer *playeri, std::vector<CBasePlayer *> &checkVec) {
        for (auto &event : kill_events) {
            for (int j = 0; j < 3; j++) {
                if (event.usedPlayerIndexes[j] == i) {
                    return false;
                }
            }
        }

        for (auto player : checkVec) {
            if (player == playeri) {
                return false;
            }
        }

        return true;
    }
    
    CBasePlayer *FindFreeFakePlayer(std::vector<CBasePlayer *> &checkVec) 
    {
        // Find a player to use, first a bot, then a real player
        for (int i = DEFAULT_MAX_PLAYERS; i >= 1; i--) {

            auto playeri = UTIL_PlayerByIndex(i);
            if (playeri == nullptr) continue;

            if (playeri->IsRealPlayer()) continue;
            if (!CanUseThisPlayerForChangeName(i, playeri, checkVec)) continue;

            return playeri;
        }

        for (int i = DEFAULT_MAX_PLAYERS; i >= 1; i--) {

            auto playeri = UTIL_PlayerByIndex(i);
            if (playeri == nullptr) continue;
            if (!CanUseThisPlayerForChangeName(i, playeri, checkVec)) continue;

            return playeri;
        }

        return nullptr;
    }

    bool stopMoreEvents = false;
    DETOUR_DECL_MEMBER(bool, IGameEventManager2_FireEvent, IGameEvent *event, bool bDontBroadcast)
	{
        stopMoreEvents = false;
        //return DETOUR_MEMBER_CALL(IGameEventManager2_FireEvent)(event, bDontBroadcast);
        Msg("stop %s\n", event != nullptr ? event->GetName() : "f");
        return false;
    }

    MemberFuncThunk<CBaseClient *, void, IGameEvent *>  ft_CBaseClient_FireGameEvent("CBaseClient::FireGameEvent");

    bool sending_delayed_event = false;
    DETOUR_DECL_MEMBER(void, CBaseClient_FireGameEvent, IGameEvent *event)
	{
        if (event->GetName() != nullptr && strcmp(event->GetName(), "player_connect_client") == 0) {
            if (event->GetInt("index") > DEFAULT_MAX_PLAYERS - 1) {
                return;
            }
        }
        if (event->GetName() != nullptr && strcmp(event->GetName(), "player_hurt") == 0) {
            IGameEvent *npchurt = gameeventmanager->CreateEvent("npc_hurt");
            auto victim = UTIL_PlayerByUserId(event->GetInt("userid"));
            if (npchurt && victim != nullptr) {
				npchurt->SetInt("entindex", victim->entindex());
				npchurt->SetInt("health", Max(0, victim->GetHealth()));
				npchurt->SetInt("damageamount", event->GetInt("damageamount"));
				npchurt->SetBool("crit", event->GetBool("crit"));
				npchurt->SetInt("attacker_player", event->GetInt("attacker"));
				npchurt->SetInt("weaponid", event->GetInt("weaponid"));

				gameeventmanager->FireEvent(npchurt);
			}
        }
        // Replace extra slot client id with some other bot with lower id
        IGameEvent *duplicate = nullptr;
        const char *eventName = nullptr;
        if (!sending_delayed_event && event != nullptr && sig_etc_extra_player_slots_allow_bots.GetBool() ) {
            eventName = event->GetName();
        }
        if ( eventName != nullptr && (strcmp(eventName, "player_death") == 0 || strcmp(eventName, "object_destroyed") == 0
              || StringStartsWith(eventName, "fish_notice"))) {

            auto victim = UTIL_PlayerByUserId(event->GetInt("userid"));
            auto attacker = UTIL_PlayerByUserId(event->GetInt("attacker"));
            auto assister = UTIL_PlayerByUserId(event->GetInt("assister"));
            if (ENTINDEX(victim) > DEFAULT_MAX_PLAYERS || ENTINDEX(attacker) > DEFAULT_MAX_PLAYERS || ENTINDEX(assister) > DEFAULT_MAX_PLAYERS) {
                auto &killevent = kill_events.emplace_back();
                duplicate = gameeventmanager->DuplicateEvent(event);
                killevent.time = gpGlobals->curtime;
                killevent.event = duplicate;
                if (ENTINDEX(victim) > DEFAULT_MAX_PLAYERS) {
                    killevent.names[0] = victim->GetPlayerName();
                    killevent.teams[0] = victim->GetTeamNumber();
                }
                else {
                    killevent.participants.push_back(victim);
                }
                if (ENTINDEX(attacker) > DEFAULT_MAX_PLAYERS) {
                    killevent.names[1] = attacker->GetPlayerName();
                    killevent.teams[1] = attacker->GetTeamNumber();
                }
                else {
                    killevent.participants.push_back(attacker);
                }
                if (ENTINDEX(assister) > DEFAULT_MAX_PLAYERS) {
                    killevent.names[2] = assister->GetPlayerName();
                    killevent.teams[2] = assister->GetTeamNumber();
                }
                else {
                    killevent.participants.push_back(assister);
                }
                return;
            }
        }
        auto client = reinterpret_cast<CBaseClient *>(this);
        DETOUR_MEMBER_CALL(CBaseClient_FireGameEvent)(event);
    }

    DETOUR_DECL_STATIC(void, SV_ComputeClientPacks, int clientCount,  void **clients, void *snapshot)
	{
        int realTeam = -1;
		if (!kill_events.empty()) {

            for (auto it = kill_events.begin(); it != kill_events.end();){
                auto &killEvent = (*it);


                if (!killEvent.prepared) {
                    
                    // Stale events over 2 seconds, remove
                    if (gpGlobals->curtime - killEvent.time > 2.0f) {
                        gameeventmanager->FreeEvent(killEvent.event);
                        it = kill_events.erase(it);
                        continue;
                    }

                    bool hasFoundAll = true;
                    CBasePlayer *players[3];
                    auto restrictedPlayers = killEvent.participants;
                    for (size_t i = 0; i < 3; i++) {
                        if (killEvent.teams[i] != -1) {
                            if (killEvent.teams[i] == killEvent.teams[i - 1] && killEvent.names[i] == killEvent.names[i - 1]) {
                                players[i] = players[i - 1];
                                continue;
                            }
                            auto player = FindFreeFakePlayer(restrictedPlayers);
                            restrictedPlayers.push_back(player);
                            if (player == nullptr && gpGlobals->curtime - killEvent.time > 1.9f) {
                                player = UTIL_PlayerByIndex(DEFAULT_MAX_PLAYERS);
                            }
                            players[i] = player;
                            if (player == nullptr){
                                hasFoundAll = false;
                            }
                        }
                    }
                    if (hasFoundAll) {
                        for (size_t i = 0; i < 3; i++) {
                            if (killEvent.teams[i] != -1) {
                                auto player = players[i];
                                killEvent.oldNames[i] = player->GetPlayerName();
                                killEvent.usedPlayerIndexes[i] = player->entindex();
                                killEvent.event->SetInt(i == 0 ? "userid" : (i == 1 ? "attacker" : "assister"), player->GetUserID());
                                
                                player->SetPlayerName(killEvent.names[i].c_str());
                                auto clientFakePlayer = static_cast<CBaseClient *>(sv->GetClient(player->entindex()-1));
                                clientFakePlayer->m_ConVars->SetString("name", killEvent.names[i].c_str());
                                V_strncpy(clientFakePlayer->m_Name, killEvent.names[i].c_str(), sizeof(clientFakePlayer->m_Name));
                                clientFakePlayer->m_bConVarsChanged = true;
                                static_cast<CBaseServer *>(sv)->UserInfoChanged(clientFakePlayer->m_nClientSlot);
                            }
                        }
                        killEvent.time = gpGlobals->curtime;
                        killEvent.prepared = true;
                    }
                    else {
                        it++;
                        continue;
                    }
                }
                
                if (gpGlobals->curtime - killEvent.time > 0.2f && !killEvent.send) {
                    killEvent.send = true;
                    sending_delayed_event = true;
                    for (int i = 0; i < sv->GetMaxClients(); i++) {
                        auto client = static_cast<CBaseClient *>(sv->GetClient(i));
                        if (client != nullptr && client->IsConnected() && !client->IsFakeClient()) {
                            ft_CBaseClient_FireGameEvent(client, killEvent.event);
                        }
                    }
                    sending_delayed_event = false;

                    gameeventmanager->FreeEvent(killEvent.event);
                }

                for (int i = 0; i < 3; i++) {
                    if (killEvent.usedPlayerIndexes[i] != -1) {
                        TFPlayerResource()->m_iTeam.SetIndex(killEvent.teams[i], killEvent.usedPlayerIndexes[i]);
                    }
                }

                if (gpGlobals->curtime - killEvent.time > 0.35f) {
                    
                    for (int i = 0; i < 3; i++) {
                        if (killEvent.usedPlayerIndexes[i] != -1 && killEvent.usedPlayerIndexes[i] != DEFAULT_MAX_PLAYERS) {
                            auto player = UTIL_PlayerByIndex(killEvent.usedPlayerIndexes[i]);
                            if (player != nullptr) {
                                auto &oldName = killEvent.oldNames[i];
                                player->SetPlayerName(oldName.c_str());
                                auto clientFakePlayer = static_cast<CBaseClient *>(sv->GetClient(player->entindex()-1));
                                V_strncpy(clientFakePlayer->m_Name, oldName.c_str(), sizeof(clientFakePlayer->m_Name));
                                clientFakePlayer->m_ConVars->SetString("name", oldName.c_str());
                                clientFakePlayer->m_bConVarsChanged = true;
                                static_cast<CBaseServer *>(sv)->UserInfoChanged(clientFakePlayer->m_nClientSlot);
                                //engine->SetFakeClientConVarValue(INDEXENT(killEvent.fakePlayersIndex[i]), "name", killEvent.fakePlayersOldNames[i].c_str());
                            }
                        }
                    }

                    it = kill_events.erase(it);
                }
                else {
                    it++;
                }
            }
        }
        //if (player33_fake_kill_time + 2 > gpGlobals->curtime) {
        //    realTeam = TFPlayerResource()->m_iTeam[DEFAULT_MAX_PLAYERS];
        //}
		DETOUR_STATIC_CALL(SV_ComputeClientPacks)(clientCount, clients, snapshot);
        //if (realTeam != -1) {
//
        //    TFPlayerResource()->m_iTeam.SetIndex(realTeam, DEFAULT_MAX_PLAYERS);
        //}
	}

    DETOUR_DECL_MEMBER(void, CSteam3Server_SendUpdatedServerDetails)
	{
        //int &players = static_cast<CBaseServer *>(sv)->GetMaxClientsRef();
        //int oldPlayers = players;
        //if (players > DEFAULT_MAX_PLAYERS) {
        //    players = DEFAULT_MAX_PLAYERS;
        //}
        int val = FixSlotCrashPre();
        DETOUR_MEMBER_CALL(CSteam3Server_SendUpdatedServerDetails)();
        FixSlotCrashPost(val);
        //players = oldPlayers;
    }

    DETOUR_DECL_MEMBER_CALL_CONVENTION(__gcc_regcall, void, CTFGameRules_CalcDominationAndRevenge, CTFPlayer *pAttacker, CBaseEntity *pWeapon, CTFPlayer *pVictim, bool bIsAssist, int *piDeathFlags)
	{
        if (ENTINDEX(pAttacker) > DEFAULT_MAX_PLAYERS || ENTINDEX(pVictim) > DEFAULT_MAX_PLAYERS) return;

        DETOUR_MEMBER_CALL(CTFGameRules_CalcDominationAndRevenge)(pAttacker, pWeapon, pVictim, bIsAssist, piDeathFlags);
    }



    DETOUR_DECL_MEMBER(const char *, CTFBot_GetNextSpawnClassname)
	{
        auto bot = reinterpret_cast<CTFBot *>(this);
        auto team = TFTeamMgr()->GetTeam(bot->GetTeamNumber());
        if (team != nullptr && team->GetNumPlayers() >= 16) {
            return g_aRawPlayerClassNames[(ENTINDEX(bot)/2 % 9) + 1];
        }
		return DETOUR_MEMBER_CALL(CTFBot_GetNextSpawnClassname)();
	}

	DETOUR_DECL_MEMBER(void, CTFPlayer_Event_Killed, const CTakeDamageInfo& info)
	{
		auto player = reinterpret_cast<CTFPlayer *>(this);
		DETOUR_MEMBER_CALL(CTFPlayer_Event_Killed)(info);
        if (sig_etc_extra_player_slots_no_death_cam.GetBool() && ToTFPlayer(player->m_hObserverTarget) != nullptr && ENTINDEX(player->m_hObserverTarget) > DEFAULT_MAX_PLAYERS) {
            player->m_hObserverTarget = nullptr;
        }
#ifdef FAKE_PLAYER_CLASS
        if (ToBaseObject(player->m_hObserverTarget) != nullptr && ENTINDEX(ToBaseObject(player->m_hObserverTarget)->GetBuilder()) > DEFAULT_MAX_PLAYERS) {
            player->m_hObserverTarget = nullptr;
        }
#endif
	}
    
    std::string nextmap;
    DETOUR_DECL_STATIC(bool, Host_Changelevel, bool fromSave, const char *mapname, const char *start)
	{
        nextmap = mapname;
		return DETOUR_STATIC_CALL(Host_Changelevel)(fromSave, mapname, start);
    }
    

	DETOUR_DECL_MEMBER(void, CTriggerCatapult_OnLaunchedVictim, CBaseEntity *pVictim)
	{
        if (pVictim != nullptr && pVictim->IsPlayer() && pVictim->entindex() > DEFAULT_MAX_PLAYERS) {
            auto catapult = reinterpret_cast<CTriggerCatapult *>(this);
            catapult->m_flRefireDelay.Get()[0] = gpGlobals->curtime + 0.5f;
            return;
        }
		DETOUR_MEMBER_CALL(CTriggerCatapult_OnLaunchedVictim)(pVictim);
	}

#ifdef FAKE_PLAYER_CLASS
    GlobalThunk<ServerClass> g_CBasePlayer_ClassReg("g_CBasePlayer_ClassReg");
    GlobalThunk<ServerClass> g_CBaseCombatCharacter_ClassReg("g_CBaseCombatCharacter_ClassReg");
    GlobalThunk<ServerClass> g_CBaseCombatWeapon_ClassReg("g_CBaseCombatWeapon_ClassReg");
    GlobalThunk<ServerClass> g_CBaseAnimating_ClassReg("g_CBaseAnimating_ClassReg");
    DETOUR_DECL_MEMBER(void, CServerGameClients_ClientPutInServer, edict_t *edict, const char *playername)
	{
        DETOUR_MEMBER_CALL(CServerGameClients_ClientPutInServer)(edict, playername);
        if (edict->m_EdictIndex > DEFAULT_MAX_PLAYERS)
            GetContainingEntity(edict)->NetworkProp()->m_pServerClass = &g_CBaseCombatCharacter_ClassReg.GetRef();
    }

    DETOUR_DECL_MEMBER(void, CBaseServer_FillServerInfo, SVC_ServerInfo &serverinfo)
	{
        DETOUR_MEMBER_CALL(CBaseServer_FillServerInfo)(serverinfo);
        serverinfo.m_nMaxClients = DEFAULT_MAX_PLAYERS;
    }

    THINK_FUNC_DECL(FixUpItemModelThink)
    {
        auto weapon = reinterpret_cast<CBaseCombatWeapon *>(this);
        auto mod = this->GetOrCreateEntityModule<Mod::Etc::Mapentity_Additions::FakePropModule>("fakeprop");
        mod->props["m_nModelIndex"] = {Variant(weapon->m_iWorldModelIndex.Get()), Variant(weapon->m_iWorldModelIndex.Get())};
        this->SetNextThink(gpGlobals->curtime+0.1f, "FixUpItemModelThink");
    }

    DETOUR_DECL_MEMBER(void, CBaseCombatWeapon_Equip, CBaseCombatCharacter *owner)
	{
		CBaseCombatWeapon *weapon = reinterpret_cast<CBaseCombatWeapon *>(this); 
		DETOUR_MEMBER_CALL(CBaseCombatWeapon_Equip)(owner);
        if (owner != nullptr && owner->IsPlayer() && owner->entindex() > DEFAULT_MAX_PLAYERS) {
            weapon->NetworkProp()->m_pServerClass = &g_CBaseAnimating_ClassReg.GetRef();

            THINK_FUNC_SET(weapon, FixUpItemModelThink, gpGlobals->curtime);
        }
    }

    
	DETOUR_DECL_MEMBER(void, CTFWearable_Equip, CBasePlayer *player)
	{
		CTFWearable *wearable = reinterpret_cast<CTFWearable *>(this); 
		DETOUR_MEMBER_CALL(CTFWearable_Equip)(player);
        if (player != nullptr && player->entindex() > DEFAULT_MAX_PLAYERS) {
            wearable->NetworkProp()->m_pServerClass = &g_CBaseAnimating_ClassReg.GetRef();
        }
    }

    DETOUR_DECL_MEMBER(void, CTFPlayer_CreateRagdollEntity, bool bShouldGib, bool bBurning, bool bUberDrop, bool bOnGround, bool bYER, bool bGold, bool bIce, bool bAsh, int iCustom, bool bClassic)
	{
        auto player = reinterpret_cast<CTFPlayer *>(this);
        if (player->entindex() > DEFAULT_MAX_PLAYERS) {
            if (bShouldGib) {
                CPVSFilter filter(player->GetAbsOrigin());
                bf_write *msg = engine->UserMessageBegin(&filter, usermessages->LookupUserMessage("BreakModel"));
                msg->WriteShort(player->GetModelIndex());
                msg->WriteBitVec3Coord(player->GetAbsOrigin());
                msg->WriteBitAngles(player->GetAbsAngles());
                msg->WriteShort(player->m_nSkin);
                engine->MessageEnd();
                player->AddEffects(EF_NODRAW);
            }
            return;
        }
		DETOUR_MEMBER_CALL(CTFPlayer_CreateRagdollEntity)(bShouldGib, bBurning, bUberDrop, bOnGround, bYER, bGold, bIce, bAsh, iCustom, bClassic);
	}

    DETOUR_DECL_MEMBER(void, CTFPlayer_CreateFeignDeathRagdoll, const CTakeDamageInfo& info, bool b1, bool b2, bool b3)
	{
        auto player = reinterpret_cast<CTFPlayer *>(this);
        if (player->entindex() > DEFAULT_MAX_PLAYERS) return;
		DETOUR_MEMBER_CALL(CTFPlayer_CreateFeignDeathRagdoll)(info, b1, b2, b3);
	}
    
    VHOOK_DECL(bool, CWeaponMedigun_Deploy)
	{
        static auto attrDef = GetItemSchema()->GetAttributeDefinitionByName("medigun particle");
        if (attrDef == nullptr) {
            attrDef = GetItemSchema()->GetAttributeDefinitionByName("medigun particle");
        }
        auto medigun = reinterpret_cast<CWeaponMedigun *>(this);
        auto effect = medigun->GetItem()->GetAttributeList().GetAttributeByID(attrDef->GetIndex());
        if (effect == nullptr && attrDef != nullptr) {
            medigun->GetItem()->GetAttributeList().AddStringAttribute(attrDef, "medicgun_beam");
        }
		return VHOOK_CALL(CWeaponMedigun_Deploy)();
	}

    int customDamageTypeBullet = 0;
	RefCount rc_FireWeaponDoTrace;
    bool isCritTrace = false;
	DETOUR_DECL_MEMBER(void, CTFPlayer_FireBullet, CTFWeaponBase *weapon, FireBulletsInfo_t& info, bool bDoEffects, int nDamageType, int nCustomDamageType)
	{
		bool doTrace = false;
        auto player = reinterpret_cast<CTFPlayer *>(this);
		if (player->entindex() > DEFAULT_MAX_PLAYERS) {
			static int	tracerCount;
			bDoEffects = true;
			int ePenetrateType = weapon ? weapon->GetPenetrateType() : TF_DMG_CUSTOM_NONE;
			if (ePenetrateType == TF_DMG_CUSTOM_NONE)
				ePenetrateType = customDamageTypeBullet;
            isCritTrace = nDamageType & DMG_CRITICAL;
			doTrace = ( ( info.m_iTracerFreq != 0 ) && ( tracerCount++ % info.m_iTracerFreq ) == 0 ) || (ePenetrateType == TF_DMG_CUSTOM_PENETRATE_ALL_PLAYERS);
		}

		SCOPED_INCREMENT_IF(rc_FireWeaponDoTrace, doTrace);

		
		DETOUR_MEMBER_CALL(CTFPlayer_FireBullet)(weapon, info, bDoEffects, nDamageType, nCustomDamageType);
	}

	DETOUR_DECL_MEMBER(void, CTFPlayer_MaybeDrawRailgunBeam, IRecipientFilter *filter, CTFWeaponBase *weapon, const Vector &vStartPos, const Vector &vEndPos)
	{
		if (rc_FireWeaponDoTrace) {
			te_tf_particle_effects_control_point_t cp;
			cp.m_eParticleAttachment = PATTACH_ABSORIGIN;
			cp.m_vecOffset = vEndPos;
            auto tracerName = reinterpret_cast<CTFPlayer *>(this)->GetTracerType();
			DispatchParticleEffect(isCritTrace ? CFmtStr("%s_crit", tracerName).Get() : tracerName, PATTACH_ABSORIGIN, nullptr, nullptr, vStartPos, true, vec3_origin, vec3_origin, false, false, &cp, nullptr);
		}
		DETOUR_MEMBER_CALL(CTFPlayer_MaybeDrawRailgunBeam)(filter, weapon, vStartPos, vEndPos);
	}

    int GetPlayerSkinNumber(CTFPlayer *player) {
        if (player->IsForcedSkin()) {
            return player->GetForcedSkin();
        }
        int team = player->GetTeamNumber();
        if (player->m_Shared->InCond(TF_COND_DISGUISED)) {
            team = player->m_Shared->m_nDisguiseTeam;
        }

        int skin = team == TF_TEAM_BLUE ? 1 : 0;

        if (player->m_Shared->IsInvulnerable() && (!player->m_Shared->InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED) || gpGlobals->curtime < player->m_flMvMLastDamageTime + 2.0f)) {
            skin += 2;
        }
        if (player->m_iPlayerSkinOverride == 1) {
            skin += player->GetPlayerClass()->GetClassIndex() == TF_CLASS_SPY ? 22 : 4;
        }
        return skin;
    }
#endif

	class CMod : public IMod, public IModCallbackListener, IFrameUpdatePostEntityThinkListener
	{
	public:
		CMod() : IMod("Perf:Extra_Player_Slots")
		{

			MOD_ADD_DETOUR_MEMBER(CBaseServer_CreateFakeClient,              "CBaseServer::CreateFakeClient");
			MOD_ADD_DETOUR_MEMBER(CBaseServer_GetFreeClient,              "CBaseServer::GetFreeClient");
			MOD_ADD_DETOUR_MEMBER(CServerGameClients_GetPlayerLimits, "CServerGameClients::GetPlayerLimits");
			MOD_ADD_DETOUR_MEMBER(CLagCompensationManager_FrameUpdatePostEntityThink, "CLagCompensationManager::FrameUpdatePostEntityThink");
            MOD_ADD_DETOUR_MEMBER(CLagCompensationManager_StartLagCompensation,  "CLagCompensationManager::StartLagCompensation");
            MOD_ADD_DETOUR_MEMBER(CLagCompensationManager_BacktrackPlayer,  "CLagCompensationManager::BacktrackPlayer");
			MOD_ADD_DETOUR_MEMBER(CLagCompensationManager_FinishLagCompensation, "CLagCompensationManager::FinishLagCompensation");
			//MOD_ADD_DETOUR_STATIC(SendProxy_PlayerList,    "SendProxy_PlayerList");
		    MOD_ADD_DETOUR_STATIC(SendProxyArrayLength_PlayerArray,    "SendProxyArrayLength_PlayerArray");
			MOD_ADD_DETOUR_MEMBER(CTeam_AddPlayer, "CTeam::AddPlayer");
			//MOD_ADD_DETOUR_MEMBER(CTeam_RemovePlayer, "CTeam::RemovePlayer");
			MOD_ADD_DETOUR_MEMBER(CGameMovement_GetPointContentsCached, "CGameMovement::GetPointContentsCached [clone]");

            
			MOD_ADD_DETOUR_MEMBER(CTFGameStats_ResetPlayerStats, "CTFGameStats::ResetPlayerStats");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_ResetKillHistory, "CTFGameStats::ResetKillHistory");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_IncrementStat, "CTFGameStats::IncrementStat");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_SendStatsToPlayer, "CTFGameStats::SendStatsToPlayer");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_AccumulateAndResetPerLifeStats, "CTFGameStats::AccumulateAndResetPerLifeStats");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_Event_PlayerConnected, "CTFGameStats::Event_PlayerConnected");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_Event_PlayerDisconnectedTF, "CTFGameStats::Event_PlayerDisconnectedTF");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_Event_PlayerLeachedHealth, "CTFGameStats::Event_PlayerLeachedHealth");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_TrackKillStats, "CTFGameStats::TrackKillStats");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_FindPlayerStats, "CTFGameStats::FindPlayerStats");
            MOD_ADD_DETOUR_MEMBER(CTFGameStats_Event_PlayerEarnedKillStreak, "CTFGameStats::Event_PlayerEarnedKillStreak");

            MOD_ADD_DETOUR_MEMBER(CTFPlayerShared_IsPlayerDominated, "CTFPlayerShared::IsPlayerDominated");
            MOD_ADD_DETOUR_MEMBER(CTFPlayerShared_IsPlayerDominatingMe, "CTFPlayerShared::IsPlayerDominatingMe");
            MOD_ADD_DETOUR_MEMBER(CTFPlayerShared_SetPlayerDominated, "CTFPlayerShared::SetPlayerDominated");
            MOD_ADD_DETOUR_MEMBER(CTFPlayerShared_SetPlayerDominatingMe, "CTFPlayerShared::SetPlayerDominatingMe");
            MOD_ADD_DETOUR_MEMBER(CTFPlayerResource_SetPlayerClassWhenKilled, "CTFPlayerResource::SetPlayerClassWhenKilled");
            MOD_ADD_DETOUR_MEMBER(CBasePlayer_UpdatePlayerSound, "CBasePlayer::UpdatePlayerSound");
            MOD_ADD_DETOUR_MEMBER(CSoundEnt_Initialize, "CSoundEnt::Initialize");
            MOD_ADD_DETOUR_MEMBER(CVoiceGameMgr_ClientConnected, "CVoiceGameMgr::ClientConnected");
            //MOD_ADD_DETOUR_MEMBER(CTFTeam_SetTeamLeader, "CTFTeam::SetTeamLeader");
            MOD_ADD_DETOUR_MEMBER(CHLTVDirector_BuildActivePlayerList, "CHLTVDirector::BuildActivePlayerList");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_HandleCommand_JoinTeam, "CTFPlayer::HandleCommand_JoinTeam");
            MOD_ADD_DETOUR_MEMBER(CTFGameRules_PlayerReadyStatus_UpdatePlayerState, "CTFGameRules::PlayerReadyStatus_UpdatePlayerState");
            MOD_ADD_DETOUR_MEMBER(CTFGCServerSystem_ClientDisconnected, "CTFGCServerSystem::ClientDisconnected");
            //MOD_ADD_DETOUR_MEMBER(CTFPlayer_UpdateOnRemove, "CTFPlayer::UpdateOnRemove");
            MOD_ADD_DETOUR_MEMBER(CTFGameRules_ClientDisconnected, "CTFGameRules::ClientDisconnected");
            MOD_ADD_DETOUR_MEMBER(CTFGameRules_GetTeamAssignmentOverride, "CTFGameRules::GetTeamAssignmentOverride");
            //MOD_ADD_DETOUR_MEMBER(ConVar_GetName, "ConVar::GetName");
            //MOD_ADD_DETOUR_MEMBER(ConCommandBase_GetName, "ConCommandBase::GetName");
            MOD_ADD_DETOUR_STATIC(SV_BroadcastVoiceData, "SV_BroadcastVoiceData");
            MOD_ADD_DETOUR_MEMBER(CVoiceGameMgrHelper_CanPlayerHearPlayer, "CVoiceGameMgrHelper::CanPlayerHearPlayer");
            MOD_ADD_DETOUR_MEMBER(CVoiceGameMgr_UpdateMasks, "CVoiceGameMgr::UpdateMasks");
            MOD_ADD_DETOUR_MEMBER(CTeamplayRoundBasedRules_State_Enter_PREROUND, "CTeamplayRoundBasedRules::State_Enter_PREROUND");
            //MOD_ADD_DETOUR_MEMBER(CTFGameRules_PowerupModeKillCountCompare, "CTFGameRules::PowerupModeKillCountCompare");
            //MOD_ADD_DETOUR_MEMBER(CTFGameRules_PowerupModeInitKillCountTimer, "CTFGameRules::PowerupModeInitKillCountTimer");

            //MOD_ADD_DETOUR_MEMBER(IGameEventManager2_FireEvent, "IGameEventManager2::FireEvent");
            MOD_ADD_DETOUR_MEMBER(CBaseClient_FireGameEvent, "CBaseClient::FireGameEvent");
            MOD_ADD_DETOUR_MEMBER(CGameMovement_CheckStuck, "CGameMovement::CheckStuck");
            MOD_ADD_DETOUR_MEMBER(CSteam3Server_SendUpdatedServerDetails, "CSteam3Server::SendUpdatedServerDetails");
            MOD_ADD_DETOUR_MEMBER(CTFGameRules_CalcDominationAndRevenge, "CTFGameRules::CalcDominationAndRevenge [clone]");
            
            MOD_ADD_DETOUR_STATIC(SV_ComputeClientPacks, "SV_ComputeClientPacks");
            MOD_ADD_DETOUR_MEMBER(CTFBot_GetNextSpawnClassname, "CTFBot::GetNextSpawnClassname");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_Event_Killed, "CTFPlayer::Event_Killed");
            MOD_ADD_DETOUR_STATIC(Host_Changelevel, "Host_Changelevel");
            
            MOD_ADD_DETOUR_MEMBER(CTriggerCatapult_OnLaunchedVictim, "CTriggerCatapult::OnLaunchedVictim");
            
#ifdef FAKE_PLAYER_CLASS
            MOD_ADD_DETOUR_MEMBER(CServerGameClients_ClientPutInServer, "CServerGameClients::ClientPutInServer");

            MOD_ADD_DETOUR_MEMBER(CBaseServer_FillServerInfo, "CBaseServer::FillServerInfo");
            MOD_ADD_DETOUR_MEMBER(CBaseCombatWeapon_Equip, "CBaseCombatWeapon::Equip");
            MOD_ADD_DETOUR_MEMBER(CTFWearable_Equip, "CTFWearable::Equip");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_CreateRagdollEntity, "CTFPlayer::CreateRagdollEntity [args]");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_CreateFeignDeathRagdoll, "CTFPlayer::CreateFeignDeathRagdoll");
            MOD_ADD_VHOOK(CWeaponMedigun_Deploy, TypeName<CWeaponMedigun>(), "CWeaponMedigun::Deploy");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_FireBullet, "CTFPlayer::FireBullet");
            MOD_ADD_DETOUR_MEMBER(CTFPlayer_MaybeDrawRailgunBeam, "CTFPlayer::MaybeDrawRailgunBeam");
#endif
            
			//MOD_ADD_DETOUR_MEMBER(CTFPlayer_ShouldTransmit,               "CTFPlayer::ShouldTransmit");
            //MOD_ADD_DETOUR_STATIC(SendTable_CalcDelta,   "SendTable_CalcDelta");
		}

		virtual void PreLoad() override
		{
            //this->AddDetour(new CDetour("CCvar::FindVar", (void *)(LibMgr::GetInfo(Library::VSTDLIB).BaseAddr() + 0x0010470), GET_MEMBER_CALLBACK(CCvar_FindVar), GET_MEMBER_INNERPTR(CCvar_FindVar)));
		}

        virtual bool ShouldReceiveCallbacks() const override { return this->IsEnabled(); }

        void PrepareLagCompensation()
        {
            // Setup additional lag compensation managers
            for (auto manager : lag_compensation_copies) {
                delete manager;
            }
            lag_compensation_copies.clear();
            for (int i = 1; i < (gpGlobals->maxClients + DEFAULT_MAX_PLAYERS - 1) / DEFAULT_MAX_PLAYERS; i++) {
                auto copiedLagManager = reinterpret_cast<CLagCompensationManager *>(::operator new(0x1FFFF));
                memcpy(copiedLagManager, &g_LagCompensationManager.GetRef(), 0x1FFFF);
                lag_compensation_copies.push_back(copiedLagManager);
            }
        }
        virtual void OnEnablePost() override
		{
            world_edict = INDEXENT(0);
            PrepareLagCompensation();
        }

        virtual void FrameUpdatePostEntityThink() override
        {
            if (gpGlobals->maxClients > VOICE_MAX_PLAYERS && sig_etc_extra_player_slots_voice_display_fix.GetBool()) {
                for (int i = 1; i <= gpGlobals->maxClients; i++) {
                    auto player = UTIL_PlayerByIndex(i);
                    if (player != nullptr && player->IsRealPlayer()) {
                        float timeSinceStartTalk = gpGlobals->curtime - talk_start_time[i - 1];
                        float timeSinceTalk = gpGlobals->curtime - talk_time[i - 1];
                        if (timeSinceStartTalk > 0 && timeSinceStartTalk < gpGlobals->frametime * 2) {
                            gamehelpers->HintTextMsg(i, "Please wait 3 seconds before talking");
                        }
                        else if (timeSinceStartTalk > 1 && timeSinceStartTalk < 1 + gpGlobals->frametime * 2) {
                            gamehelpers->HintTextMsg(i, "Please wait 2 seconds before talking");
                        }
                        else if (timeSinceStartTalk > 2 && timeSinceStartTalk < 2 + gpGlobals->frametime * 2) {
                            gamehelpers->HintTextMsg(i, "Please wait 1 seconds before talking");
                        }
                        else if (timeSinceStartTalk > 3 && timeSinceStartTalk < 3 + gpGlobals->frametime * 2) {
                            gamehelpers->HintTextMsg(i, "You may talk now");
                        }
                        else if (timeSinceTalk > 5 && timeSinceTalk < 5 + gpGlobals->frametime) {
                            gamehelpers->HintTextMsg(i, " ");
                        }
                        else if (timeSinceTalk >= 5 + gpGlobals->frametime && timeSinceTalk < 5 + gpGlobals->frametime * 2) {
                            gamehelpers->HintTextMsg(i, "");
                        }
                    }
                }
            }
#ifdef FAKE_PLAYER_CLASS
            if (ExtraSlotsEnabled()) {
                for (int i = DEFAULT_MAX_PLAYERS + 1; i <= gpGlobals->maxClients; i++) {
                    auto player = ToTFPlayer(UTIL_PlayerByIndex(i));
                    if (player != nullptr) {
                        if (player->m_hRagdoll != nullptr) {
                            player->m_hRagdoll->Remove();
                            player->m_hRagdoll = nullptr;
                        }

                        
                        bool isRed = player->GetTeamNumber() == TF_TEAM_RED;
                        bool isScoped = player->IsAlive() && player->m_Shared->InCond(TF_COND_AIMING) && rtti_cast<CTFSniperRifle *>(player->GetActiveWeapon()) != nullptr;
                        bool wasScoped = player->GetCustomVariableBool<"wasscoped">();
                        if (isScoped != wasScoped) {
                            if (isScoped) {
                                auto sprite = CSprite::SpriteCreate("sprites/glow01.vmt", player->GetAbsOrigin(), false);
                                sprite->KeyValue("scale", "0.3");

                                auto laser = CBeam::BeamCreate("sprites/laser.vmt", 2.0f);
                                laser->EntsInit(laser, sprite);
                                player->SetCustomVariable<"beament">(Variant((CBaseEntity *) laser));
                                player->SetCustomVariable<"beamspriteent">(Variant((CBaseEntity *) sprite));
                                sprite->SetRenderMode(kRenderTransAdd);
                                sprite->SetRenderColorR(isRed ? 255 : 0);
                                sprite->SetRenderColorG(isRed ? 0 : 0);
                                sprite->SetRenderColorB(isRed ? 0 : 255);
                                laser->SetRenderColorR(isRed ? 255 : 0);
                                laser->SetRenderColorG(isRed ? 0 : 0);
                                laser->SetRenderColorB(isRed ? 0 : 255);
                                laser->SetParent(player, -1);
                                sprite->SetParent(player, -1);
                                laser->SetAbsOrigin(player->EyePosition());
                            }
                            else {
                                variant_t beam;
                                if (player->GetCustomVariableVariant<"beament">(beam) && beam.Entity() != nullptr) {
                                    beam.Entity()->Remove();
                                }
                                variant_t sprite;
                                if (player->GetCustomVariableVariant<"beamspriteent">(sprite) && sprite.Entity() != nullptr) {
                                    sprite.Entity()->Remove();
                                }

                            }
                            player->SetCustomVariable<"wasscoped">(Variant(isScoped));
                        }

                        if (!player->IsAlive() && gpGlobals->curtime - player->GetDeathTime() < 0.05f) continue;

                        player->m_bClientSideAnimation = player->m_Shared->InCond(TF_COND_TAUNTING);
                        bool refreshBodyGroup = false;
                        
                        for (int i = 0; i < player->WeaponCount(); i++) {
                            auto weapon = player->GetWeapon(i);
                            if (weapon != nullptr) {
                                weapon->m_nSkin = static_cast<CTFWeaponBase *>(weapon)->GetSkin();
                                weapon->m_nSequence = 0;
                                if (!weapon->GetCustomVariableBool<"equipextraslots">()) {
                                    weapon->SetCustomVariable<"equipextraslots">(Variant(true));
                                    refreshBodyGroup = true;
                                }
                                bool isCurrentWeapon = weapon == player->GetActiveWeapon();
                                bool wasCurrentWeapon = weapon->GetCustomVariableBool<"deployextraslots">();
                                if (isCurrentWeapon != wasCurrentWeapon) {
                                    weapon->SetCustomVariable<"deployextraslots">(Variant(isCurrentWeapon));
                                    refreshBodyGroup = true;
                                }
                            }
                        }
                        
                        for (int i = 0; i < player->GetNumWearables(); i++) {
                            auto wearable = player->GetWearable(i);
                            if (wearable != nullptr) {
                                wearable->m_nSkin = wearable->GetSkin();
                                if (!wearable->GetCustomVariableBool<"equipextraslots">()) {
                                    wearable->SetCustomVariable<"equipextraslots">(Variant(true));
                                    refreshBodyGroup = true;
                                }
                            }
                        }

                        if (refreshBodyGroup) {
                            player->m_Shared->RecalculatePlayerBodygroups();
                        }
                        bool isCritical = player->m_Shared->IsCritBoosted();
                        bool isMiniCritical = player->m_Shared->InCond(TF_COND_OFFENSEBUFF) || player->m_Shared->InCond(TF_COND_ENERGY_BUFF);
                        constexpr color32 critWeaponColorRed(255,60,50,255);
                        constexpr color32 critWeaponColorBlu(40,70,255,255);
                        constexpr color32 baseColor(255,255,255,255);
                        color32 desiredWeaponColor = baseColor;
                        variant_t oldWeaponColorVariant;
                        player->m_nSkin = GetPlayerSkinNumber(player);
                        player->GetCustomVariableVariant<"oldweaponcolor">(oldWeaponColorVariant);
                        color32 oldWeaponColor = oldWeaponColorVariant.Color32();
                        if (isCritical) {
                            desiredWeaponColor = isRed ? critWeaponColorRed : critWeaponColorBlu;
                        }
                        else if (isMiniCritical) {
                            desiredWeaponColor = isRed ? color32(255, 140, 60, 255) : color32(70, 150, 255, 255);
                        }
                        if (desiredWeaponColor != oldWeaponColor) {
                            for (int i = 0; i < player->WeaponCount(); i++) {
                                auto weapon = player->GetWeapon(i);
                                if (weapon != nullptr) {
                                    auto color = weapon->GetRenderColor();
                                    if (color.r == oldWeaponColor.r && color.g == oldWeaponColor.g && color.b == oldWeaponColor.b) {
                                        weapon->SetRenderColorR(desiredWeaponColor.r);
                                        weapon->SetRenderColorG(desiredWeaponColor.g);
                                        weapon->SetRenderColorB(desiredWeaponColor.b);
                                    }
                                    if (isCritical) {
                                        DispatchParticleEffect(isRed ? "critgun_weaponmodel_red" : "critgun_weaponmodel_blu", PATTACH_ABSORIGIN_FOLLOW, weapon, -1, false);
                                    }
                                    else {
                                        StopParticleEffects(weapon);
                                    }
                                }
                            }
                            player->SetCustomVariable<"oldweaponcolor">(Variant(desiredWeaponColor));
                        }
                        
                        constexpr color32 jarateColor(255,255,70,255);
                        bool isJarated = player->m_Shared->InCond(TF_COND_URINE);
                        bool wasJarated = player->GetCustomVariableBool<"wasjarated">();
                        if (isJarated != wasJarated) {
                            color32 desiredColor = isJarated ? jarateColor : baseColor;
                            color32 oldColor = isJarated ? baseColor : jarateColor;
                            auto color = player->GetRenderColor();
                            if (color.r == oldColor.r && color.g == oldColor.g && color.b == oldColor.b) {
                                player->SetRenderColorR(desiredColor.r);
                                player->SetRenderColorG(desiredColor.g);
                                player->SetRenderColorB(desiredColor.b);
                            }
                            if (isJarated) {
                                DispatchParticleEffect("peejar_drips", PATTACH_ABSORIGIN_FOLLOW, player, -1, false);
                            }
                            else {
                                StopParticleEffects(player);
                            }
                            player->SetCustomVariable<"wasjarated">(Variant(isJarated));
                        }

                        bool isMilked = player->m_Shared->InCond(TF_COND_MAD_MILK);
                        bool wasMilked = player->GetCustomVariableBool<"wasmilked">();
                        if (isMilked != wasMilked) {
                            if (isMilked) {
                                DispatchParticleEffect("peejar_drips_milk", PATTACH_ABSORIGIN_FOLLOW, player, -1, false);
                            }
                            else {
                                StopParticleEffects(player);
                            }
                            player->SetCustomVariable<"wasmilked">(Variant(isMilked));
                        }

                        if (isScoped) {
                            variant_t sprite;
                            if (player->GetCustomVariableVariant<"beamspriteent">(sprite) && sprite.Entity() != nullptr) {
                                
                                Vector forward;
                                player->EyeVectors(&forward);
                                trace_t trace;
		                        UTIL_TraceLine(player->EyePosition(), player->EyePosition() + forward * 8192, MASK_SOLID_BRUSHONLY, player, COLLISION_GROUP_NONE, &trace);
                                sprite.Entity()->SetAbsOrigin(trace.endpos);
                            }
                        }
                    }
                }
            }
#endif
            if (ExtraSlotsEnabled() && sig_etc_extra_player_slots_show_player_names.GetBool() && (sig_etc_extra_player_slots_allow_bots.GetBool() || sig_etc_extra_player_slots_allow_players.GetBool())) {
                for (int i = 1 + gpGlobals->tickcount % 2; i <= gpGlobals->maxClients; i+=2) {
                    auto player = (CTFPlayer *)(world_edict + i)->GetUnknown();
                    if (player != nullptr && player->IsRealPlayer()) {
                        CBaseEntity *target = nullptr; 
                        if ((player->GetObserverMode() == OBS_MODE_DEATHCAM || player->GetObserverMode() == OBS_MODE_CHASE) && player->m_hObserverTarget != nullptr && player->m_hObserverTarget != player) {
                            target = player->m_hObserverTarget;
                        }
                        else {
                            trace_t tr;
                            Vector eyePos = player->EyePosition();
                            Vector start, end;
                            Vector forward;
                            player->EyeVectors(&forward);
                            VectorMA(eyePos, MAX_TRACE_LENGTH, forward, end);
                            VectorMA(eyePos, 10, forward, start);
                            UTIL_TraceLine(start, end, MASK_SOLID | CONTENTS_DEBRIS, player, COLLISION_GROUP_NONE, &tr);
                            target = tr.m_pEnt;
                        }
                        auto playerHit = ToTFPlayer(target);
                        auto objectHit = ToBaseObject(target);
                        if (objectHit != nullptr) {
                            playerHit = objectHit->GetBuilder();
                        }
                        if (playerHit != nullptr && playerHit->entindex() > DEFAULT_MAX_PLAYERS) {
                            if (playerHit->GetTeamNumber() == player->GetTeamNumber()) {
                                auto textParams = WhiteTextParams(5, -1, 0.65f, 0.06f);
                                DisplayHudMessageAutoChannel(player, textParams, playerHit->GetPlayerName(), 4);
                            }
                        }
                    }
                }
            }
        }

        virtual void LevelInitPreEntity() override
		{
            kill_events.clear();

            if (ExtraSlotsEnabled() && MapHasExtraSlots(STRING(gpGlobals->mapname)) && (gpGlobals->maxClients >= DEFAULT_MAX_PLAYERS - 1 && gpGlobals->maxClients < sig_etc_extra_player_slots_count.GetInt())) {
                engine->ChangeLevel(STRING(gpGlobals->mapname), nullptr);
            }
            sv_alltalk.Reset();
            for (auto &time : talk_time) {
                time = -100.0f;
            }
            for (auto &time : talk_start_time) {
                time = -100.0f;
            }
            PrepareLagCompensation();
        }

        virtual void LevelShutdownPostEntity() override
		{
            //Msg("Has %d\n", sig_etc_extra_player_slots_allow_bots.GetBool());
            bool enable = MapHasExtraSlots(nextmap.c_str());
            if (!enable && gpGlobals->maxClients >= DEFAULT_MAX_PLAYERS) {
			    //static ConVarRef tv_enable("tv_enable");
                //bool tv_enabled = tv_enable.GetBool();
                //tv_enable.SetValue(false);
                //ft_SetupMaxPlayers(DEFAULT_MAX_PLAYERS);
                //tv_enable.SetValue(tv_enabled);
            }
            else {
                if (ExtraSlotsEnabled() && (gpGlobals->maxClients >= DEFAULT_MAX_PLAYERS - 1 && gpGlobals->maxClients < sig_etc_extra_player_slots_count.GetInt())) {
                    // Kick old HLTV client
                    int clientCount = sv->GetClientCount();
                    for ( int i=0 ; i < clientCount ; i++ )
                    {
                        IClient *pClient = sv->GetClient( i );

                        if (pClient->IsConnected() && pClient->IsHLTV() && i <= DEFAULT_MAX_PLAYERS)
                        {
                            pClient->Disconnect("");
                            break;
                        }
                    }
                    ft_SetupMaxPlayers(sig_etc_extra_player_slots_count.GetInt());
                }
            }
            
        }
	};
	CMod s_Mod;
    
	ConVar cvar_enable("sig_etc_extra_player_slots", "0", FCVAR_NOTIFY,
		"Mod: allows usage of extra player slots",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			s_Mod.Toggle(static_cast<ConVar *>(pConVar)->GetBool());
		});
}