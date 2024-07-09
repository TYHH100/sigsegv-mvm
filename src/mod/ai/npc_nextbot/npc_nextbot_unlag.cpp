#include "mod.h"
#include "re/nextbot.h"
#include "stub/nextbot_cc.h"
#include "stub/nextbot_cc_behavior.h"
#include "stub/nextbot_etc.h"
#include "re/path.h"
#include "stub/tfbot.h"
#include "stub/gamerules.h"
#include "stub/tfentities.h"
#include "util/iterate.h"
#include "mod/common/commands.h"
#include "stub/extraentitydata.h"
#include "util/misc.h"
#include "util/value_override.h"
#include "mod/ai/npc_nextbot/npc_nextbot_actions.h"
#include "mod/ai/npc_nextbot/npc_nextbot.h"
#include "mod/etc/mapentity_additions.h"
#include "mod/item/item_common.h"
#include "stub/server.h"
#include "stub/usermessages_sv.h"
#include "stub/particles.h"
#include "stub/lagcompensation.h"

#define LAG_COMPENSATION_EPS_SQR ( 0.1f * 0.1f )
// Allow 4 units of error ( about 1 / 8 bbox width )
#define LAG_COMPENSATION_ERROR_EPS_SQR ( 4.0f * 4.0f )

float g_flFractionScale = 0.95;

#define LC_NONE				0
#define LC_ALIVE			(1<<0)

#define LC_ORIGIN_CHANGED	(1<<8)
#define LC_ANGLES_CHANGED	(1<<9)
#define LC_SIZE_CHANGED		(1<<10)
#define LC_ANIMATION_CHANGED (1<<11)

namespace Mod::AI::NPC_Nextbot
{
    ConVar *sv_unlag;
    ConVar *sv_maxunlag;
    ConVar *sv_lagcompensation_teleport_dist;
	ConVar *sv_lagflushbonecache;

    float flTeleportDistanceSqr = 0;
    CBasePlayer *currentPlayer = nullptr;
    bool isCurrentlyDoingCompensation = false;
    bool needToRestore = false;

    static void RestoreTo(CBaseEntity *entity, const Vector &vWantedPos)
	{
		// Try to move to the wanted position from our current position.
		trace_t tr;
		VPROF_BUDGET( "RestorePlayerTo", "CLagCompensationManager" );
        uint solidMask = entity->PhysicsSolidMaskForEntity();
        Vector delta = entity->GetAbsOrigin() - vWantedPos;
        //Msg("Lag comp restore pos %f %f %f\n", delta.x, delta.y, delta.z);
        CTraceFilterSimple filter(entity, entity->GetCollisionGroup());
        Vector mins = entity->CollisionProp()->OBBMins();
        Vector maxs = entity->CollisionProp()->OBBMaxs();
        mins.z = Min(mins.z + 6.0f, maxs.z);
		UTIL_TraceHull(vWantedPos, vWantedPos, mins, maxs, solidMask, &filter, &tr);
        //Msg("ToGroundDiff %f\n", vWantedPos.z - tr.endpos.z);
         //UTIL_TraceEntity( entity, vWantedPos+ Vector(0,0,1.0f), vWantedPos+ Vector(0,0,1.0f), solidMask, entity, entity->GetCollisionGroup(), &tr );
		if ( tr.startsolid || tr.allsolid )
		{
            if (tr.m_pEnt != nullptr) {
                Msg("Lag comp restore fail %s\n", tr.m_pEnt->GetClassname());
            }
			UTIL_TraceEntity( entity, entity->GetLocalOrigin(), vWantedPos, solidMask, entity, entity->GetCollisionGroup(), &tr );
			if ( tr.startsolid || tr.allsolid )
			{

			}
			else
			{
				// We can get to a valid place, but not all the way back to where we were.
				Vector vPos;
				VectorLerp( entity->GetLocalOrigin(), vWantedPos, tr.fraction * g_flFractionScale, vPos );
				entity->SetLocalOrigin(vPos);
			}
		}
		else
		{
			// Cool, the player can go back to whence he came.
			entity->SetLocalOrigin(tr.endpos);
            Msg("Lag comp restore success %f %f %f\n", delta.x, delta.y, delta.z);
		}
	}

    void BacktrackEntity(LagCompensatedEntity *lagcompent, float flTargetTime )
    {
		auto entity = lagcompent->entity;
        Vector org;
        Vector minsPreScaled;
        Vector maxsPreScaled;
        QAngle ang;

        VPROF_BUDGET( "BacktrackNPC", "NextBot" );

        // get track history of this player
        CUtlFixedLinkedList< LagRecord > *track = &lagcompent->track;

        // check if we have at leat one entry
        if ( track->Count() <= 0 )
            return;

        int curr = track->Head();

        LagRecord *prevRecord = NULL;
        LagRecord *record = NULL;

        Vector prevOrg = entity->GetLocalOrigin();
        
        // Walk context looking for any invalidating event
        while( track->IsValidIndex(curr) )
        {
            // remember last record
            prevRecord = record;

            // get next record
            record = &track->Element( curr );

            if ( !(record->m_fFlags & LC_ALIVE) )
            {
                // player most be alive, lost track
                return;
            }

            Vector delta = record->m_vecOrigin - prevOrg;
            if ( delta.Length2DSqr() > flTeleportDistanceSqr )
            {
                // lost track, too much difference
                return; 
            }

            // did we find a context smaller than target time ?
            if ( record->m_flSimulationTime <= flTargetTime )
                break; // hurra, stop

            prevOrg = record->m_vecOrigin;

            // go one step back
            curr = track->Next( curr );
        }

        float frac = 0.0f;
        if ( prevRecord && 
            (record->m_flSimulationTime < flTargetTime) &&
            (record->m_flSimulationTime < prevRecord->m_flSimulationTime) )
        {
            // we didn't find the exact time but have a valid previous record
            // so interpolate between these two records;

            Assert( prevRecord->m_flSimulationTime > record->m_flSimulationTime );
            Assert( flTargetTime < prevRecord->m_flSimulationTime );

            // calc fraction between both records
            frac = ( flTargetTime - record->m_flSimulationTime ) / 
                ( prevRecord->m_flSimulationTime - record->m_flSimulationTime );

            Assert( frac > 0 && frac < 1 ); // should never extrapolate

            ang				= Lerp( frac, record->m_vecAngles, prevRecord->m_vecAngles );
            org				= Lerp( frac, record->m_vecOrigin, prevRecord->m_vecOrigin );
            minsPreScaled	= Lerp( frac, record->m_vecMinsPreScaled, prevRecord->m_vecMinsPreScaled );
            maxsPreScaled	= Lerp( frac, record->m_vecMaxsPreScaled, prevRecord->m_vecMaxsPreScaled );
        }
        else
        {
            // we found the exact record or no other record to interpolate with
            // just copy these values since they are the best we have
            org				= record->m_vecOrigin;
            ang				= record->m_vecAngles;
            minsPreScaled	= record->m_vecMinsPreScaled;
            maxsPreScaled	= record->m_vecMaxsPreScaled;
        }

        // See if this represents a change for the player
        int flags = 0;
        LagRecord *restore = &lagcompent->restoreData; //m_RestoreData[ pl_index ];
        LagRecord *change  = &lagcompent->changeData;

        QAngle angdiff = entity->GetLocalAngles() - ang;
        Vector orgdiff = entity->GetLocalOrigin() - org;

        // Always remember the pristine simulation time in case we need to restore it.
        restore->m_flSimulationTime = entity->m_flSimulationTime;

        if ( angdiff.LengthSqr() > LAG_COMPENSATION_EPS_SQR )
        {
            flags |= LC_ANGLES_CHANGED;
            restore->m_vecAngles = entity->GetLocalAngles();
            entity->SetLocalAngles( ang );
            change->m_vecAngles = ang;
        }

        // Use absolute equality here
        if ( minsPreScaled != entity->CollisionProp()->OBBMinsPreScaled() || maxsPreScaled != entity->CollisionProp()->OBBMaxsPreScaled() )
        {
            flags |= LC_SIZE_CHANGED;

            restore->m_vecMinsPreScaled = entity->CollisionProp()->OBBMinsPreScaled();
            restore->m_vecMaxsPreScaled = entity->CollisionProp()->OBBMaxsPreScaled();
            
            UTIL_SetSize(entity, &minsPreScaled, &maxsPreScaled );
            
            change->m_vecMinsPreScaled = minsPreScaled;
            change->m_vecMaxsPreScaled = maxsPreScaled;
        }

        // Note, do origin at end since it causes a relink into the k/d tree
        if ( orgdiff.LengthSqr() > LAG_COMPENSATION_EPS_SQR )
        {
            flags |= LC_ORIGIN_CHANGED;
            restore->m_vecOrigin = entity->GetLocalOrigin();
            entity->SetLocalOrigin( org );
            change->m_vecOrigin = org;
        }

        // Sorry for the loss of the optimization for the case of people
        // standing still, but you breathe even on the server.
        // This is quicker than actually comparing all bazillion floats.
        flags |= LC_ANIMATION_CHANGED;
        restore->m_masterSequence = entity->m_nSequence;
        restore->m_masterCycle = entity->GetCycle();

        bool interpolationAllowed = false;
        if( prevRecord && (record->m_masterSequence == prevRecord->m_masterSequence) )
        {
            // If the master state changes, all layers will be invalid too, so don't interp (ya know, interp barely ever happens anyway)
            interpolationAllowed = true;
        }
        
        ////////////////////////
        // First do the master settings
        bool interpolatedMasters = false;
        if( frac > 0.0f && interpolationAllowed )
        {
            interpolatedMasters = true;
            entity->m_nSequence = Lerp( frac, record->m_masterSequence, prevRecord->m_masterSequence );
            entity->SetCycle( Lerp( frac, record->m_masterCycle, prevRecord->m_masterCycle ) );

            if( record->m_masterCycle > prevRecord->m_masterCycle )
            {
                // the older record is higher in frame than the newer, it must have wrapped around from 1 back to 0
                // add one to the newer so it is lerping from .9 to 1.1 instead of .9 to .1, for example.
                float newCycle = Lerp( frac, record->m_masterCycle, prevRecord->m_masterCycle + 1 );
                entity->SetCycle(newCycle < 1 ? newCycle : newCycle - 1 );// and make sure .9 to 1.2 does not end up 1.05
            }
            else
            {
                entity->SetCycle( Lerp( frac, record->m_masterCycle, prevRecord->m_masterCycle ) );
            }
        }
        if( !interpolatedMasters )
        {
            entity->m_nSequence = record->m_masterSequence;
            entity->SetCycle(record->m_masterCycle);
        }

        ////////////////////////
        // Now do all the layers
        int layerCount = entity->m_AnimOverlay->Count();
        for( int layerIndex = 0; layerIndex < layerCount; ++layerIndex )
        {
            CAnimationLayer *currentLayer = &entity->m_AnimOverlay.Get()[layerIndex];
            if( currentLayer )
            {
                restore->m_layerRecords[layerIndex].m_cycle = currentLayer->m_flCycle;
                restore->m_layerRecords[layerIndex].m_order = currentLayer->m_nOrder;
                restore->m_layerRecords[layerIndex].m_sequence = currentLayer->m_nSequence;
                restore->m_layerRecords[layerIndex].m_weight = currentLayer->m_flWeight;

                bool interpolated = false;
                if( (frac > 0.0f)  &&  interpolationAllowed )
                {
                    LayerRecord &recordsLayerRecord = record->m_layerRecords[layerIndex];
                    LayerRecord &prevRecordsLayerRecord = prevRecord->m_layerRecords[layerIndex];
                    if( (recordsLayerRecord.m_order == prevRecordsLayerRecord.m_order)
                        && (recordsLayerRecord.m_sequence == prevRecordsLayerRecord.m_sequence)
                        )
                    {
                        // We can't interpolate across a sequence or order change
                        interpolated = true;
                        if( recordsLayerRecord.m_cycle > prevRecordsLayerRecord.m_cycle )
                        {
                            // the older record is higher in frame than the newer, it must have wrapped around from 1 back to 0
                            // add one to the newer so it is lerping from .9 to 1.1 instead of .9 to .1, for example.
                            float newCycle = Lerp( frac, recordsLayerRecord.m_cycle, prevRecordsLayerRecord.m_cycle + 1 );
                            currentLayer->m_flCycle = newCycle < 1 ? newCycle : newCycle - 1;// and make sure .9 to 1.2 does not end up 1.05
                        }
                        else
                        {
                            currentLayer->m_flCycle = Lerp( frac, recordsLayerRecord.m_cycle, prevRecordsLayerRecord.m_cycle  );
                        }
                        currentLayer->m_nOrder = recordsLayerRecord.m_order;
                        currentLayer->m_nSequence = recordsLayerRecord.m_sequence;
                        currentLayer->m_flWeight = Lerp( frac, recordsLayerRecord.m_weight, prevRecordsLayerRecord.m_weight  );
                    }
                }
                if( !interpolated )
                {
                    //Either no interp, or interp failed.  Just use record.
                    currentLayer->m_flCycle = record->m_layerRecords[layerIndex].m_cycle;
                    currentLayer->m_nOrder = record->m_layerRecords[layerIndex].m_order;
                    currentLayer->m_nSequence = record->m_layerRecords[layerIndex].m_sequence;
                    currentLayer->m_flWeight = record->m_layerRecords[layerIndex].m_weight;
                }
            }
        }
        
        if ( !flags )
            return; // we didn't change anything

        if ( sv_lagflushbonecache->GetBool() )
            entity->InvalidateBoneCache();

        /*char text[256]; Q_snprintf( text, sizeof(text), "time %.2f", flTargetTime );
        pPlayer->DrawServerHitboxes( 10 );
        NDebugOverlay::Text( org, text, false, 10 );
        NDebugOverlay::EntityBounds( pPlayer, 255, 0, 0, 32, 10 ); */

        lagcompent->restore = true;
        needToRestore = true;
        restore->m_fFlags = flags; // we need to restore these flags
        change->m_fFlags = flags; // we have changed these flags
    }

	
	DETOUR_DECL_MEMBER(void, CLagCompensationManager_FinishLagCompensation, CBasePlayer *player)
	{
		DETOUR_MEMBER_CALL(CLagCompensationManager_FinishLagCompensation)(player);
		if (reinterpret_cast<CLagCompensationManager *>(this) != &g_LagCompensationManager.GetRef()) return;

		VPROF_BUDGET_FLAGS( "FinishNPCLagCompensation", VPROF_BUDGETGROUP_OTHER_NETWORKING, BUDGETFLAG_CLIENT|BUDGETFLAG_SERVER );

		currentPlayer = nullptr;

		if (!needToRestore) {
			isCurrentlyDoingCompensation = false;
			return;
		}

		for (LagCompensatedEntity *lagcompent : AutoList<LagCompensatedEntity>::List()) {
            auto entity = lagcompent->entity;
			
            Msg("Lag comp restore %d\n", lagcompent->restore);
			if (!lagcompent->restore) continue;

			LagRecord *restore = &lagcompent->restoreData;
			LagRecord *change  = &lagcompent->changeData;

			bool restoreSimulationTime = false;

			if ( restore->m_fFlags & LC_SIZE_CHANGED )
			{
				restoreSimulationTime = true;
		
				// see if simulation made any changes, if no, then do the restore, otherwise,
				//  leave new values in
				if ( entity->CollisionProp()->OBBMinsPreScaled() == change->m_vecMinsPreScaled &&
					entity->CollisionProp()->OBBMaxsPreScaled() == change->m_vecMaxsPreScaled )
				{
					// Restore it
					UTIL_SetSize(entity, &restore->m_vecMinsPreScaled, &restore->m_vecMaxsPreScaled );
				}
	#ifdef STAGING_ONLY
				else
				{
					Warning( "Should we really not restore the size?\n" );
				}
	#endif
			}

			if ( restore->m_fFlags & LC_ANGLES_CHANGED )
			{		   
				restoreSimulationTime = true;

				if ( entity->GetLocalAngles() == change->m_vecAngles )
				{
					entity->SetLocalAngles( restore->m_vecAngles );
				}
			}

			if ( restore->m_fFlags & LC_ORIGIN_CHANGED )
			{
				restoreSimulationTime = true;

				// Okay, let's see if we can do something reasonable with the change
				Vector delta = entity->GetLocalOrigin() - change->m_vecOrigin;
				
				// If it moved really far, just leave the player in the new spot!!!
				if ( delta.Length2DSqr() < flTeleportDistanceSqr )
				{
					RestoreTo( entity, restore->m_vecOrigin + delta );
				}
			}

			if( restore->m_fFlags & LC_ANIMATION_CHANGED )
			{
				restoreSimulationTime = true;

				entity->m_nSequence = restore->m_masterSequence;
				entity->SetCycle(restore->m_masterCycle);

				int layerCount = entity->m_AnimOverlay->Count();
				for( int layerIndex = 0; layerIndex < layerCount; ++layerIndex )
				{
					CAnimationLayer *currentLayer = &entity->m_AnimOverlay.Get()[layerIndex];
					if( currentLayer )
					{
						currentLayer->m_flCycle = restore->m_layerRecords[layerIndex].m_cycle;
						currentLayer->m_nOrder = restore->m_layerRecords[layerIndex].m_order;
						currentLayer->m_nSequence = restore->m_layerRecords[layerIndex].m_sequence;
						currentLayer->m_flWeight = restore->m_layerRecords[layerIndex].m_weight;
					}
				}
			}

			if ( restoreSimulationTime )
			{
				entity->m_flSimulationTime = restore->m_flSimulationTime;
			}
		}

		isCurrentlyDoingCompensation = false;
	}

	bool LagCompensatedEntity::WantsLagCompensation(CBasePlayer *player, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits)
	{
		if (pEntityTransmitBits && !pEntityTransmitBits->Get(entity->entindex())) return false;

		const Vector &vMyOrigin = player->GetAbsOrigin();
		const Vector &vHisOrigin = entity->GetAbsOrigin();

		float maxDistance = 1.5 * entity->GetAbsVelocity().Length() * sv_maxunlag->GetFloat();

		// If the player is within this distance, lag compensate them in case they're running past us.
		if ( vHisOrigin.DistTo( vMyOrigin ) < maxDistance )
			return true;

		// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
		Vector vForward;
		AngleVectors( pCmd->viewangles, &vForward );

		Vector vDiff = vHisOrigin - vMyOrigin;
		VectorNormalize( vDiff );

		float flCosAngle = 0.707107f;	// 45 degree angle
		if ( vForward.Dot( vDiff ) < flCosAngle ) return false;

		return true;
	}

	bool MyNextbotModule::WantsLagCompensation(CBasePlayer *player, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits)
	{
		if (!LagCompensatedEntity::WantsLagCompensation(player, pCmd, pEntityTransmitBits)) return false;

		return this->m_bAllowTakeFriendlyFire || this->m_pEntity->GetTeamNumber() != player->GetTeamNumber();
	}
    // Called during player movement to set up/restore after lag compensation

	DETOUR_DECL_MEMBER(void, CLagCompensationManager_StartLagCompensation, CBasePlayer *player, CUserCmd *cmd)
	{
		DETOUR_MEMBER_CALL(CLagCompensationManager_StartLagCompensation)(player, cmd);
		if (reinterpret_cast<CLagCompensationManager *>(this) != &g_LagCompensationManager.GetRef()) return;

        if (cmd == nullptr) return;
        //DONT LAG COMP AGAIN THIS FRAME IF THERES ALREADY ONE IN PROGRESS
        //IF YOU'RE HITTING THIS THEN IT MEANS THERES A CODE BUG
        if (currentPlayer)
        {
            Warning( "Trying to start a new lag compensation session while one is already active!\n" );
            return;
        }

        needToRestore = false;

        currentPlayer = player;
        if ( !player->m_bLagCompensation		// Player not wanting lag compensation
            || (gpGlobals->maxClients <= 1)	// no lag compensation in single player
            || !sv_unlag->GetBool()				// disabled by server admin
            || player->IsBot() 				// not for bots
            || player->IsObserver()			// not for spectators
            )
            return;

        // NOTE: Put this here so that it won't show up in single player mode.
        VPROF_BUDGET( "StartNPCLagCompensation", "NextBot" );

        isCurrentlyDoingCompensation = true;

        // Get true latency

        // correct is the amout of time we have to correct game time
        float correct = 0.0f;

        INetChannelInfo *nci = engine->GetPlayerNetInfo( player->entindex() ); 

        if ( nci )
        {
            // add network latency
            correct+= nci->GetLatency( FLOW_OUTGOING );
        }

        // calc number of view interpolation ticks - 1
        int lerpTicks = TIME_TO_TICKS( player->m_fLerpTime );

        // add view interpolation latency see C_BaseEntity::GetInterpolationAmount()
        correct += TICKS_TO_TIME( lerpTicks );
        
        // check bouns [0,sv_maxunlag]
        correct = clamp( correct, 0.0f, sv_maxunlag->GetFloat() );

        // correct tick send by player 
        int targettick = cmd->tick_count - lerpTicks;

        // calc difference between tick send by player and our latency based tick
        float deltaTime =  correct - TICKS_TO_TIME(gpGlobals->tickcount - targettick);

        if ( fabs( deltaTime ) > 0.2f )
        {
            // difference between cmd time and latency is too big > 200ms, use time correction based on latency
            // DevMsg("StartLagCompensation: delta too big (%.3f)\n", deltaTime );
            targettick = gpGlobals->tickcount - TIME_TO_TICKS( correct );
        }
        
        const CBitVec<MAX_EDICTS> *pEntityTransmitBits = engine->GetEntityTransmitBitsForClient( player->entindex() - 1 );
        for (LagCompensatedEntity *lagcompent : AutoList<LagCompensatedEntity>::List()) {
            auto ent = lagcompent->entity;
            lagcompent->restore = false;
            Q_memset(&lagcompent->restoreData, 0, sizeof(LagRecord));
            Q_memset(&lagcompent->changeData, 0, sizeof(LagRecord));

            // Custom checks for if things should lag compensate (based on things like what team the player is on).
            if ( !lagcompent->WantsLagCompensation( player, cmd, pEntityTransmitBits ) )
                continue;

            Vector pre = ent->GetAbsOrigin();
            // Move npc back in time
            BacktrackEntity( lagcompent, TICKS_TO_TIME( targettick ) );
        }
    }

    
    class CModUnlag : public IMod, public IFrameUpdatePostEntityThinkListener
	{
	public:
		CModUnlag() : IMod("AI:My_Nextbot_Unlag")
		{
            MOD_ADD_DETOUR_MEMBER(CLagCompensationManager_StartLagCompensation,  "CLagCompensationManager::StartLagCompensation");
			MOD_ADD_DETOUR_MEMBER(CLagCompensationManager_FinishLagCompensation, "CLagCompensationManager::FinishLagCompensation");
		}

        virtual bool OnLoad() override
        {
            sv_unlag = g_pCVar->FindVar("sv_unlag");
            sv_maxunlag = g_pCVar->FindVar("sv_maxunlag");
            sv_lagcompensation_teleport_dist = g_pCVar->FindVar("sv_lagcompensation_teleport_dist");
			sv_lagflushbonecache = g_pCVar->FindVar("sv_lagcompensation_teleport_dist");
            return true;
        }

        virtual bool ShouldReceiveCallbacks() const override { return this->IsEnabled(); }
		
		virtual void FrameUpdatePostEntityThink() override
		{
            if (AutoList<LagCompensatedEntity>::List().empty()) return;

	        flTeleportDistanceSqr = sv_lagcompensation_teleport_dist->GetFloat() * sv_lagcompensation_teleport_dist->GetFloat();

            float flDeadtime = gpGlobals->curtime - sv_maxunlag->GetFloat();
            for (LagCompensatedEntity *lagcompent : AutoList<LagCompensatedEntity>::List()) {
                auto ent = lagcompent->entity;
                auto track = &lagcompent->track;

                Assert( track->Count() < 1000 ); // insanity check

                // remove tail records that are too old
                int tailIndex = track->Tail();
                while ( track->IsValidIndex( tailIndex ) )
                {
                    LagRecord &tail = track->Element( tailIndex );

                    // if tail is within limits, stop
                    if ( tail.m_flSimulationTime >= flDeadtime )
                        break;
                    
                    // remove tail, get new tail
                    track->Remove( tailIndex );
                    tailIndex = track->Tail();
                }

                // check if head has same simulation time
                if ( track->Count() > 0 )
                {
                    LagRecord &head = track->Element( track->Head() );

                    // check if player changed simulation time since last time updated
                    if ( head.m_flSimulationTime >= ent->m_flSimulationTime )
                        continue; // don't add new entry for same or older time
                }

                // add new record to player track
                LagRecord &record = track->Element( track->AddToHead() );

                record.m_fFlags = 0;
                if ( ent->IsAlive() )
                {
                    record.m_fFlags |= LC_ALIVE;
                }

                record.m_flSimulationTime	= ent->m_flSimulationTime;
                record.m_vecAngles			= ent->GetLocalAngles();
                record.m_vecOrigin			= ent->GetLocalOrigin();
                record.m_vecMinsPreScaled	= ent->CollisionProp()->OBBMinsPreScaled();
                record.m_vecMaxsPreScaled	= ent->CollisionProp()->OBBMaxsPreScaled();

                int layerCount = ent->m_AnimOverlay->Count();
                for( int layerIndex = 0; layerIndex < layerCount; ++layerIndex )
                {
                    CAnimationLayer *currentLayer = &ent->m_AnimOverlay.Get()[layerIndex];
                    if( currentLayer )
                    {
                        record.m_layerRecords[layerIndex].m_cycle = currentLayer->m_flCycle;
                        record.m_layerRecords[layerIndex].m_order = currentLayer->m_nOrder;
                        record.m_layerRecords[layerIndex].m_sequence = currentLayer->m_nSequence;
                        record.m_layerRecords[layerIndex].m_weight = currentLayer->m_flWeight;
                    }
                }
                record.m_masterSequence = ent->m_nSequence;
                record.m_masterCycle = ent->GetCycle();
            }
            currentPlayer = nullptr;
        }
	};
	CModUnlag s_ModUnlag;

    class LagCompensationModule : public EntityModule, public LagCompensatedEntity
    {
    public:
        LagCompensationModule(CBaseEntity *entity) : EntityModule(entity), LagCompensatedEntity((CBaseAnimatingOverlay *)entity) {}
    };

    DETOUR_DECL_MEMBER(void, NextBotCombatCharacter_Spawn)
	{
		DETOUR_MEMBER_CALL(NextBotCombatCharacter_Spawn)();
        auto entity = reinterpret_cast<NextBotCombatCharacter *>(this);
        if (!FStrEq(entity->GetClassname(), "$bot_npc")) {
            entity->GetOrCreateEntityModule<LagCompensationModule>("lagcomp");
        }
    }

    class CModUnlagNPCGlobal : public IMod
	{
	public:
		CModUnlagNPCGlobal() : IMod("AI:My_Nextbot_UnlagNPCGlobal")
		{
            MOD_ADD_DETOUR_MEMBER(NextBotCombatCharacter_Spawn,  "NextBotCombatCharacter::Spawn");
		}
		virtual std::vector<std::string> GetRequiredMods() { return {"AI:My_Nextbot_Unlag"};}
	};
	CModUnlagNPCGlobal s_ModUnlagNPCGlobal;

    ConVar cvar_enable_unlagnpcglobal("sig_ai_lag_compensation_npc", "0", FCVAR_NOTIFY | FCVAR_GAMEDLL,
		"Mod: Lag compensate all NPC entities such as bosses, zombies etc",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			s_ModUnlagNPCGlobal.Toggle(static_cast<ConVar *>(pConVar)->GetBool());
		});
}