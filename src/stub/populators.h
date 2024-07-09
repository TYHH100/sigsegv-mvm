#ifndef _INCLUDE_SIGSEGV_STUB_POPULATORS_H_
#define _INCLUDE_SIGSEGV_STUB_POPULATORS_H_


#include "link/link.h"
#include "stub/tfbot.h"


struct EventInfo
{
	CFmtStrN<256> target; // +0x000
	CFmtStrN<256> action; // +0x10c
	CFmtStrN<256> param; // +0x10c
	float delay;
};

class CWave;
class CUpgradeInfo;
class PlayerUpgradeHistory;

class CPopulationManager : public CPointEntity
{
public:
	bool LoadMvMMission(KeyValues *kv) { return ft_LoadMvMMission(this, kv); }
	CWave *GetCurrentWave()            { return ft_GetCurrentWave(this); }
	void ResetMap()                    {        ft_ResetMap      (this); }
	void PauseSpawning()               {        ft_PauseSpawning (this); }
	void UnpauseSpawning()             {        ft_UnpauseSpawning (this); }
	void AllocateBots()	               {        ft_AllocateBots (this); }
	bool IsInEndlessWaves()	           { return ft_IsInEndlessWaves (this); }
	static int CollectMvMBots(CUtlVector<CTFPlayer *> *mvm_bots)                 { return ft_CollectMvMBots(mvm_bots); }
	void RemovePlayerAndItemUpgradesFromHistory( CTFPlayer *pPlayer )            { return ft_RemovePlayerAndItemUpgradesFromHistory(this, pPlayer); }
	static void FindDefaultPopulationFileShortNames(CUtlVector<CUtlString> &vec) { return ft_FindDefaultPopulationFileShortNames(vec); }
	CUtlVector< CUpgradeInfo > * GetPlayerUpgradeHistory(CTFPlayer *player)      { return ft_GetPlayerUpgradeHistory(this, player); }
	void SetPopulationFilename(const char *name)                                 { ft_SetPopulationFilename(this, name); }
	int GetPlayerCurrencySpent(CTFPlayer *player)                                { return ft_GetPlayerCurrencySpent(this, player); }
	void JumpToWave(int wave, float money = -1.0f)                               {        ft_JumpToWave (this, wave, money); }
	PlayerUpgradeHistory * FindOrAddPlayerUpgradeHistory(CSteamID steamid)	     { return ft_FindOrAddPlayerUpgradeHistory (this, steamid); }
	void SetCheckpoint(int wave)                                                 {        ft_SetCheckpoint (this, wave); }
	
	using SteamIDMap = CUtlMap<uint64_t, int>;
	DECL_EXTRACT(SteamIDMap, m_RespecPoints);
	DECL_EXTRACT(bool,       m_bAllocatedBots);
	DECL_RELATIVE(KeyValues *,m_pTemplates);
	DECL_RELATIVE(int,       m_nStartingCurrency);
	
private:
	static MemberFuncThunk<CPopulationManager *, bool, KeyValues *> ft_LoadMvMMission;
	static MemberFuncThunk<CPopulationManager *, CWave *>           ft_GetCurrentWave;
	static MemberFuncThunk<CPopulationManager *, void>              ft_ResetMap;
	static MemberFuncThunk<CPopulationManager *, void>              ft_PauseSpawning;
	static MemberFuncThunk<CPopulationManager *, void>              ft_UnpauseSpawning;
	static MemberFuncThunk<CPopulationManager *, void>              ft_AllocateBots;
	static MemberFuncThunk<CPopulationManager *, bool>              ft_IsInEndlessWaves;
	static MemberFuncThunk<CPopulationManager *, void, CTFPlayer *>             ft_RemovePlayerAndItemUpgradesFromHistory;
	static MemberFuncThunk<CPopulationManager *, CUtlVector< CUpgradeInfo > *, CTFPlayer *>             ft_GetPlayerUpgradeHistory;
	static MemberFuncThunk<CPopulationManager *, void, const char*> ft_SetPopulationFilename;
	static MemberFuncThunk<CPopulationManager *, int, CTFPlayer *> ft_GetPlayerCurrencySpent;
	static MemberFuncThunk<CPopulationManager *, void, int, float> ft_JumpToWave;
	static MemberFuncThunk<CPopulationManager *, PlayerUpgradeHistory *, CSteamID> ft_FindOrAddPlayerUpgradeHistory;
	static MemberFuncThunk<CPopulationManager *, void, int> ft_SetCheckpoint;
	
	static StaticFuncThunk<int, CUtlVector<CTFPlayer *> *> ft_CollectMvMBots;
	static StaticFuncThunk<void, CUtlVector<CUtlString> &> ft_FindDefaultPopulationFileShortNames;
};
extern GlobalThunk<CPopulationManager *> g_pPopulationManager;


class IPopulationSpawner;


class IPopulator
{
public:
	virtual ~IPopulator() {};
	IPopulationSpawner *m_Spawner;
	CPopulationManager *m_PopMgr;
};


class CRandomPlacementPopulator : public IPopulator {};
class CPeriodicSpawnPopulator   : public IPopulator {};

class CSpawnLocation
{
public:
	int m_relative;
	CUtlVector< CHandle< CTFTeamSpawn > > m_teamSpawnVector;

	int m_nSpawnCount;
	int m_nRandomSeed;
	bool m_bClosestPointOnNav;
};

class CWaveSpawnExtra;

enum SpawnLocationResult
{
	SPAWN_LOCATION_NOT_FOUND = 0,
	SPAWN_LOCATION_NAV,
	SPAWN_LOCATION_TELEPORTER
};

class CWaveSpawnPopulator       : public IPopulator 
{
public:
	//void **vtable;
	CSpawnLocation m_where;
	int m_totalCount;
	int m_remainingCount;
	CWaveSpawnExtra *extra;  // int m_nClassCounts;
	int m_maxActive;						
	int m_spawnCount;						
	float m_waitBeforeStarting;
	float m_waitBetweenSpawns;				
	bool m_bWaitBetweenSpawnAfterDeath;

	CFmtStr m_startWaveWarningSound;
	EventInfo *m_startWaveOutput;

	CFmtStr m_firstSpawnWarningSound;
	EventInfo *m_firstSpawnOutput;

	CFmtStr m_lastSpawnWarningSound;
	EventInfo *m_lastSpawnOutput;

	CFmtStr m_doneWarningSound;
	EventInfo *m_doneOutput;

	int		m_totalCurrency;
	int		m_unallocatedCurrency;

	CUtlString m_name;
	CUtlString m_waitForAllSpawned;
	CUtlString m_waitForAllDead;
	CountdownTimer m_timer;
	CUtlVector<CHandle<CBaseEntity>> m_activeVector;
	int m_countSpawnedSoFar;
	int m_myReservedSlotCount;
	
	bool m_bSupportWave;
	bool m_bLimitedSupport;
	CWave *m_pParent;
	
	enum InternalStateType
	{
		PENDING,
		PRE_SPAWN_DELAY,
		SPAWNING,
		WAIT_FOR_ALL_DEAD,
		DONE
	};
	InternalStateType m_state;
	
	bool m_bRandomSpawn;
	SpawnLocationResult m_spawnLocationResult;
	Vector m_vSpawnPosition;
	
	int GetCurrencyAmountPerDeath()                                       { return ft_GetCurrencyAmountPerDeath(this); }
	void SetState(CWaveSpawnPopulator::InternalStateType state)           {        ft_SetState(this, state); }

	static GlobalThunkRW<int> m_reservedPlayerSlotCount;
	
private:
	static MemberFuncThunk<CWaveSpawnPopulator *, int> ft_GetCurrencyAmountPerDeath;
	static MemberFuncThunk<CWaveSpawnPopulator *, void, CWaveSpawnPopulator::InternalStateType> ft_SetState;
};

class CRandomChoiceSpawner;

class CWaveSpawnExtra
{
public:
	bool m_bPaused = false;
	bool m_bHasTFBotSpawner = false;
	CUtlVector<CWaveSpawnPopulator *> m_waitForAllDeadList;
	CUtlVector<CWaveSpawnPopulator *> m_waitForAllSpawnedList;
	CRandomChoiceSpawner *randomChoiceShuffleSet = nullptr;
};

struct WaveClassCount_t
{
	int nClassCount;
	string_t iszClassIconName;
	unsigned int iFlags;
};

class CWave : public IPopulator
{
public:
	void AddClassType(string_t icon, int count, unsigned int flags) { ft_AddClassType      (this, icon, count, flags); }
	void ForceFinish()                                              { ft_ForceFinish       (this); }
	void ActiveWaveUpdate()                                         { ft_ActiveWaveUpdate  (this); }
	void WaveCompleteUpdate()                                       { ft_WaveCompleteUpdate(this); }
	bool IsDoneWithNonSupportWaves()                                { return ft_IsDoneWithNonSupportWaves(this); }
	//CUtlVector<CWaveSpawnPopulator *> GetWaveSpawns()	            { return reinterpret_cast<T>((uintptr_t)this + 0x18 - 0x0c); }
	
	CUtlVector<CWaveSpawnPopulator *> m_WaveSpawns;
	bool m_isStarted;
	bool m_bFiredInitWaveOutput;
	int m_iEnemyCount;
	int m_nTanksSpawned;
	int m_nSentryBustersSpawned;
	int m_nNumEngineersTeleportSpawned;

	int m_nNumSentryBustersKilled;

	CUtlVector< WaveClassCount_t > m_nWaveClassCounts;
	int	m_totalCurrency;

	EventInfo *m_startOutput;
	EventInfo *m_doneOutput;
	EventInfo *m_initOutput;

	CFmtStr m_description;
	CFmtStr m_soundName;
	
	float m_waitWhenDone;
	CountdownTimer m_doneTimer;	

	bool m_bCheckBonusCreditsMin;
	bool m_bCheckBonusCreditsMax;
	float m_flBonusCreditsTime;

	bool m_bPlayedUpgradeAlert;
	CountdownTimer m_GetUpgradesAlertTimer;

	bool m_isEveryContainedWaveSpawnDone;
	float m_flStartTime;
	
private:
	static MemberFuncThunk<CWave *, void, string_t, int, unsigned int> ft_AddClassType;
	static MemberFuncThunk<CWave *, void>                              ft_ForceFinish;
	static MemberFuncThunk<CWave *, void>                              ft_ActiveWaveUpdate;
	static MemberFuncThunk<CWave *, void>                              ft_WaveCompleteUpdate;
	static MemberFuncThunk<CWave *, bool>                              ft_IsDoneWithNonSupportWaves;
};

class CMissionPopulator : public IPopulator
{
public:
	bool UpdateMission(int mtype) { return ft_UpdateMission(this, mtype); }
	
	int m_Objective;
	
private:
	static MemberFuncThunk<CMissionPopulator *, bool, int> ft_UpdateMission;
};


class IPopulationSpawner
{
public:
	virtual ~IPopulationSpawner()
	{
	}

	IPopulationSpawner( IPopulator *populator )
	{
		m_Populator = populator;
	}

	virtual bool Parse( KeyValues *data ) = 0;
	virtual bool Spawn( const Vector &here, CUtlVector<CHandle<CBaseEntity>> *ents = NULL ) = 0;
	virtual bool IsWhereRequired( void ) const		// does this spawner need a valid Where parameter?
	{
		return true;
	}
	
	virtual bool IsVarious( void ) { return false; }
	virtual int GetClass( int nSpawnNum = -1 ) { return TF_CLASS_UNDEFINED; }
	virtual string_t GetClassIcon( int nSpawnNum = -1 ) { return NULL_STRING; }
	virtual int GetHealth( int nSpawnNum = -1  ){ return 0; }
	virtual bool IsMiniBoss( int nSpawnNum = -1 ) { return false; }
	virtual bool HasAttribute( CTFBot::AttributeType type, int nSpawnNum = -1 ) { return false; }
	virtual bool HasEventChangeAttributes( const char* pszEventName ) const = 0;

	static IPopulationSpawner *ParseSpawner(IPopulator *populator, KeyValues *kv) { return ft_ParseSpawner(populator, kv); }

	IPopulator *m_Populator;
	
private:
	static MemberVFuncThunk<IPopulationSpawner *, string_t, int                   > vt_GetClassIcon;
	static MemberVFuncThunk<IPopulationSpawner *, bool, int                       > vt_IsMiniBoss;
	static MemberVFuncThunk<IPopulationSpawner *, bool, CTFBot::AttributeType, int> vt_HasAttribute;
	static MemberVFuncThunk<IPopulationSpawner *, bool, KeyValues *               > vt_Parse;
	
	static StaticFuncThunk<IPopulationSpawner *, IPopulator *, KeyValues *> ft_ParseSpawner;
};

class CMobSpawner : public IPopulationSpawner
{
public:
	int m_iCount;
	IPopulationSpawner *m_SubSpawner;
};

class CSentryGunSpawner : public IPopulationSpawner {};
class CTankSpawner      : public IPopulationSpawner {};

class CTFBotSpawner : public IPopulationSpawner
{
public:
	int m_iClass;
	string_t m_strClassIcon;
	int m_iHealth;
	float m_flScale;
	float m_flAutoJumpMin;
	float m_flAutoJumpMax;
	CUtlString m_strName;
	CUtlStringList m_TeleportWhere;
	CTFBot::EventChangeAttributes_t m_DefaultAttrs;
	CUtlVector<CTFBot::EventChangeAttributes_t> m_ECAttrs;
	
};

class CSquadSpawner : public IPopulationSpawner
{
public:
	CUtlVector<IPopulationSpawner *> m_SubSpawners;

	float m_formationSize;
	char m_bShouldPreserveSquad;
};

class CRandomChoiceSpawner : public IPopulationSpawner
{
public:
	CUtlVector<IPopulationSpawner *> m_SubSpawners;
	CUtlVector<int> m_Indexes;
};

struct PlayerUpgradeHistory
{	
	CSteamID m_steamId;							// which player this snapshot is for
	CUtlVector< CUpgradeInfo > m_upgradeVector;	 
	int m_currencySpent;
};

extern StaticFuncThunk<bool, const Vector&> ft_IsSpaceToSpawnHere;
extern StaticFuncThunk<bool, CTFBot::EventChangeAttributes_t &, KeyValues *> ft_ParseDynamicAttributes;
extern StaticFuncThunk<int, int, bool> ft_MannVsMachineStats_GetAcquiredCredits;
extern StaticFuncThunk<void, EventInfo *, const char *> ft_FireEvent;
extern StaticFuncThunk<EventInfo *, KeyValues *> ft_ParseEvent;


inline bool IsSpaceToSpawnHere(const Vector& pos)
{
	return ft_IsSpaceToSpawnHere(pos);
}

inline bool ParseDynamicAttributes(CTFBot::EventChangeAttributes_t &event, KeyValues *kv)
{
	return ft_ParseDynamicAttributes(event, kv);
}

inline int MannVsMachineStats_GetAcquiredCredits(int wave = -1, bool bonus = true)
{
	return ft_MannVsMachineStats_GetAcquiredCredits(wave, bonus);
}

inline void FireEvent(EventInfo *info, const char *name)
{
	return ft_FireEvent(info, name);
}

inline EventInfo *ParseEvent(KeyValues *kv)
{
	return ft_ParseEvent(kv);
}
#endif
