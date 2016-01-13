#include "link/link.h"
#include "extension.h"
#include "util/util.h"
#include "stub/stub.h"
#include "re/nextbot.h"
#include "re/path.h"


#define INER INextBotEventResponder
#define AR   ActionResult<CTFBot>
#define EDR  EventDesiredResult<CTFBot>

static FuncThunk<INER * (*)(const INER *)>                                                ft_INextBotEventResponder_FirstContainedResponder(       "INextBotEventResponder::FirstContainedResponder");
static FuncThunk<INER * (*)(const INER *, INER *)>                                        ft_INextBotEventResponder_NextContainedResponder(        "INextBotEventResponder::NextContainedResponder");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnLeaveGround(                 "INextBotEventResponder::OnLeaveGround");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnLandOnGround(                "INextBotEventResponder::OnLandOnGround");
static FuncThunk<void   (*)(INER *, CBaseEntity *, CGameTrace *)>                         ft_INextBotEventResponder_OnContact(                     "INextBotEventResponder::OnContact");
static FuncThunk<void   (*)(INER *, const Path *)>                                        ft_INextBotEventResponder_OnMoveToSuccess(               "INextBotEventResponder::OnMoveToSuccess");
static FuncThunk<void   (*)(INER *, const Path *, INER::MoveToFailureType)>               ft_INextBotEventResponder_OnMoveToFailure(               "INextBotEventResponder::OnMoveToFailure");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnStuck(                       "INextBotEventResponder::OnStuck");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnUnStuck(                     "INextBotEventResponder::OnUnStuck");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnPostureChanged(              "INextBotEventResponder::OnPostureChanged");
static FuncThunk<void   (*)(INER *, int)>                                                 ft_INextBotEventResponder_OnAnimationActivityComplete(   "INextBotEventResponder::OnAnimationActivityComplete");
static FuncThunk<void   (*)(INER *, int)>                                                 ft_INextBotEventResponder_OnAnimationActivityInterrupted("INextBotEventResponder::OnAnimationActivityInterrupted");
static FuncThunk<void   (*)(INER *, animevent_t *)>                                       ft_INextBotEventResponder_OnAnimationEvent(              "INextBotEventResponder::OnAnimationEvent");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnIgnite(                      "INextBotEventResponder::OnIgnite");
static FuncThunk<void   (*)(INER *, const CTakeDamageInfo&)>                              ft_INextBotEventResponder_OnInjured(                     "INextBotEventResponder::OnInjured");
static FuncThunk<void   (*)(INER *, const CTakeDamageInfo&)>                              ft_INextBotEventResponder_OnKilled(                      "INextBotEventResponder::OnKilled");
static FuncThunk<void   (*)(INER *, CBaseCombatCharacter *, const CTakeDamageInfo&)>      ft_INextBotEventResponder_OnOtherKilled(                 "INextBotEventResponder::OnOtherKilled");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnSight(                       "INextBotEventResponder::OnSight");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnLostSight(                   "INextBotEventResponder::OnLostSight");
static FuncThunk<void   (*)(INER *, CBaseEntity *, const Vector&, KeyValues *)>           ft_INextBotEventResponder_OnSound(                       "INextBotEventResponder::OnSound");
static FuncThunk<void   (*)(INER *, CBaseCombatCharacter *, const char *, AI_Response *)> ft_INextBotEventResponder_OnSpokeConcept(                "INextBotEventResponder::OnSpokeConcept");
static FuncThunk<void   (*)(INER *, CBaseCombatCharacter *, CBaseCombatWeapon *)>         ft_INextBotEventResponder_OnWeaponFired(                 "INextBotEventResponder::OnWeaponFired");
static FuncThunk<void   (*)(INER *, CNavArea *, CNavArea *)>                              ft_INextBotEventResponder_OnNavAreaChanged(              "INextBotEventResponder::OnNavAreaChanged");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnModelChanged(                "INextBotEventResponder::OnModelChanged");
static FuncThunk<void   (*)(INER *, CBaseEntity *, CBaseCombatCharacter *)>               ft_INextBotEventResponder_OnPickUp(                      "INextBotEventResponder::OnPickUp");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnDrop(                        "INextBotEventResponder::OnDrop");
static FuncThunk<void   (*)(INER *, CBaseCombatCharacter *, int)>                         ft_INextBotEventResponder_OnActorEmoted(                 "INextBotEventResponder::OnActorEmoted");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnCommandAttack(               "INextBotEventResponder::OnCommandAttack");
static FuncThunk<void   (*)(INER *, const Vector&, float)>                                ft_INextBotEventResponder_OnCommandApproach_vec(         "INextBotEventResponder::OnCommandApproach(vec)");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnCommandApproach_ent(         "INextBotEventResponder::OnCommandApproach(ent)");
static FuncThunk<void   (*)(INER *, CBaseEntity *, float)>                                ft_INextBotEventResponder_OnCommandRetreat(              "INextBotEventResponder::OnCommandRetreat");
static FuncThunk<void   (*)(INER *, float)>                                               ft_INextBotEventResponder_OnCommandPause(                "INextBotEventResponder::OnCommandPause");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnCommandResume(               "INextBotEventResponder::OnCommandResume");
static FuncThunk<void   (*)(INER *, const char *)>                                        ft_INextBotEventResponder_OnCommandString(               "INextBotEventResponder::OnCommandString");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnShoved(                      "INextBotEventResponder::OnShoved");
static FuncThunk<void   (*)(INER *, CBaseEntity *)>                                       ft_INextBotEventResponder_OnBlinded(                     "INextBotEventResponder::OnBlinded");
static FuncThunk<void   (*)(INER *, int)>                                                 ft_INextBotEventResponder_OnTerritoryContested(          "INextBotEventResponder::OnTerritoryContested");
static FuncThunk<void   (*)(INER *, int)>                                                 ft_INextBotEventResponder_OnTerritoryCaptured(           "INextBotEventResponder::OnTerritoryCaptured");
static FuncThunk<void   (*)(INER *, int)>                                                 ft_INextBotEventResponder_OnTerritoryLost(               "INextBotEventResponder::OnTerritoryLost");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnWin(                         "INextBotEventResponder::OnWin");
static FuncThunk<void   (*)(INER *)>                                                      ft_INextBotEventResponder_OnLose(                        "INextBotEventResponder::OnLose");

static FuncThunk<QueryResponse        (*)(const IContextualQuery *, const INextBot *nextbot, CBaseEntity *)>                                                            ft_IContextualQuery_ShouldPickUp(             "IContextualQuery::ShouldPickUp");
static FuncThunk<QueryResponse        (*)(const IContextualQuery *, const INextBot *nextbot)>                                                                           ft_IContextualQuery_ShouldHurry(              "IContextualQuery::ShouldHurry");
static FuncThunk<QueryResponse        (*)(const IContextualQuery *, const INextBot *nextbot)>                                                                           ft_IContextualQuery_ShouldRetreat(            "IContextualQuery::ShouldRetreat");
static FuncThunk<QueryResponse        (*)(const IContextualQuery *, const INextBot *nextbot, const CKnownEntity *)>                                                     ft_IContextualQuery_ShouldAttack(             "IContextualQuery::ShouldAttack");
static FuncThunk<QueryResponse        (*)(const IContextualQuery *, const INextBot *nextbot, CBaseEntity *)>                                                            ft_IContextualQuery_IsHindrance(              "IContextualQuery::IsHindrance");
static FuncThunk<Vector               (*)(const IContextualQuery *, const INextBot *nextbot, const CBaseCombatCharacter *)>                                             ft_IContextualQuery_SelectTargetPoint(        "IContextualQuery::SelectTargetPoint");
static FuncThunk<QueryResponse        (*)(const IContextualQuery *, const INextBot *nextbot, const Vector&)>                                                            ft_IContextualQuery_IsPositionAllowed(        "IContextualQuery::IsPositionAllowed");
static FuncThunk<const CKnownEntity * (*)(const IContextualQuery *, const INextBot *nextbot, const CBaseCombatCharacter *, const CKnownEntity *, const CKnownEntity *)> ft_IContextualQuery_SelectMoreDangerousThreat("IContextualQuery::SelectMoreDangerousThreat");

static FuncThunk<INER *           (*)(const Action<CTFBot> *)>                                                               ft_Action_INER_FirstContainedResponder(       "Action<CTFBot>::FirstContainedResponder"        " [INER]");
static FuncThunk<INER *           (*)(const Action<CTFBot> *, INER *)>                                                       ft_Action_INER_NextContainedResponder(        "Action<CTFBot>::NextContainedResponder"         " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnLeaveGround(                 "Action<CTFBot>::OnLeaveGround"                  " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnLandOnGround(                "Action<CTFBot>::OnLandOnGround"                 " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *, CGameTrace *)>                                        ft_Action_INER_OnContact(                     "Action<CTFBot>::OnContact"                      " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, const Path *)>                                                       ft_Action_INER_OnMoveToSuccess(               "Action<CTFBot>::OnMoveToSuccess"                " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, const Path *, INER::MoveToFailureType)>                              ft_Action_INER_OnMoveToFailure(               "Action<CTFBot>::OnMoveToFailure"                " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnStuck(                       "Action<CTFBot>::OnStuck"                        " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnUnStuck(                     "Action<CTFBot>::OnUnStuck"                      " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnPostureChanged(              "Action<CTFBot>::OnPostureChanged"               " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, int)>                                                                ft_Action_INER_OnAnimationActivityComplete(   "Action<CTFBot>::OnAnimationActivityComplete"    " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, int)>                                                                ft_Action_INER_OnAnimationActivityInterrupted("Action<CTFBot>::OnAnimationActivityInterrupted" " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, animevent_t *)>                                                      ft_Action_INER_OnAnimationEvent(              "Action<CTFBot>::OnAnimationEvent"               " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnIgnite(                      "Action<CTFBot>::OnIgnite"                       " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, const CTakeDamageInfo&)>                                             ft_Action_INER_OnInjured(                     "Action<CTFBot>::OnInjured"                      " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, const CTakeDamageInfo&)>                                             ft_Action_INER_OnKilled(                      "Action<CTFBot>::OnKilled"                       " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseCombatCharacter *, const CTakeDamageInfo&)>                     ft_Action_INER_OnOtherKilled(                 "Action<CTFBot>::OnOtherKilled"                  " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnSight(                       "Action<CTFBot>::OnSight"                        " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnLostSight(                   "Action<CTFBot>::OnLostSight"                    " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *, const Vector&, KeyValues *)>                          ft_Action_INER_OnSound(                       "Action<CTFBot>::OnSound"                        " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseCombatCharacter *, const char *, AI_Response *)>                ft_Action_INER_OnSpokeConcept(                "Action<CTFBot>::OnSpokeConcept"                 " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseCombatCharacter *, CBaseCombatWeapon *)>                        ft_Action_INER_OnWeaponFired(                 "Action<CTFBot>::OnWeaponFired"                  " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CNavArea *, CNavArea *)>                                             ft_Action_INER_OnNavAreaChanged(              "Action<CTFBot>::OnNavAreaChanged"               " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnModelChanged(                "Action<CTFBot>::OnModelChanged"                 " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *, CBaseCombatCharacter *)>                              ft_Action_INER_OnPickUp(                      "Action<CTFBot>::OnPickUp"                       " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnDrop(                        "Action<CTFBot>::OnDrop"                         " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseCombatCharacter *, int)>                                        ft_Action_INER_OnActorEmoted(                 "Action<CTFBot>::OnActorEmoted"                  " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnCommandAttack(               "Action<CTFBot>::OnCommandAttack"                " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, const Vector&, float)>                                               ft_Action_INER_OnCommandApproach_vec(         "Action<CTFBot>::OnCommandApproach(vec)"         " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnCommandApproach_ent(         "Action<CTFBot>::OnCommandApproach(ent)"         " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *, float)>                                               ft_Action_INER_OnCommandRetreat(              "Action<CTFBot>::OnCommandRetreat"               " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, float)>                                                              ft_Action_INER_OnCommandPause(                "Action<CTFBot>::OnCommandPause"                 " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnCommandResume(               "Action<CTFBot>::OnCommandResume"                " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, const char *)>                                                       ft_Action_INER_OnCommandString(               "Action<CTFBot>::OnCommandString"                " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnShoved(                      "Action<CTFBot>::OnShoved"                       " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, CBaseEntity *)>                                                      ft_Action_INER_OnBlinded(                     "Action<CTFBot>::OnBlinded"                      " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, int)>                                                                ft_Action_INER_OnTerritoryContested(          "Action<CTFBot>::OnTerritoryContested"           " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, int)>                                                                ft_Action_INER_OnTerritoryCaptured(           "Action<CTFBot>::OnTerritoryCaptured"            " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *, int)>                                                                ft_Action_INER_OnTerritoryLost(               "Action<CTFBot>::OnTerritoryLost"                " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnWin(                         "Action<CTFBot>::OnWin"                          " [INER]");
static FuncThunk<void             (*)(Action<CTFBot> *)>                                                                     ft_Action_INER_OnLose(                        "Action<CTFBot>::OnLose"                         " [INER]");
static FuncThunk<bool             (*)(const Action<CTFBot> *, const char *)>                                                 ft_Action_IsNamed(                            "Action<CTFBot>::IsNamed");
static FuncThunk<char *           (*)(const Action<CTFBot> *)>                                                               ft_Action_GetFullName(                        "Action<CTFBot>::GetFullName");
static FuncThunk<AR               (*)(Action<CTFBot> *, CTFBot *actor, Action<CTFBot> *)>                                    ft_Action_OnStart(                            "Action<CTFBot>::OnStart");
static FuncThunk<AR               (*)(Action<CTFBot> *, CTFBot *actor, float)>                                               ft_Action_Update(                             "Action<CTFBot>::Update");
static FuncThunk<void             (*)(Action<CTFBot> *, CTFBot *actor, Action<CTFBot> *)>                                    ft_Action_OnEnd(                              "Action<CTFBot>::OnEnd");
static FuncThunk<AR               (*)(Action<CTFBot> *, CTFBot *actor, Action<CTFBot> *)>                                    ft_Action_OnSuspend(                          "Action<CTFBot>::OnSuspend");
static FuncThunk<AR               (*)(Action<CTFBot> *, CTFBot *actor, Action<CTFBot> *)>                                    ft_Action_OnResume(                           "Action<CTFBot>::OnResume");
static FuncThunk<Action<CTFBot> * (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_InitialContainedAction(             "Action<CTFBot>::InitialContainedAction");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnLeaveGround(                      "Action<CTFBot>::OnLeaveGround");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnLandOnGround(                     "Action<CTFBot>::OnLandOnGround");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *, CGameTrace *)>                         ft_Action_OnContact(                          "Action<CTFBot>::OnContact");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, const Path *)>                                        ft_Action_OnMoveToSuccess(                    "Action<CTFBot>::OnMoveToSuccess");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, const Path *, INER::MoveToFailureType)>               ft_Action_OnMoveToFailure(                    "Action<CTFBot>::OnMoveToFailure");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnStuck(                            "Action<CTFBot>::OnStuck");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnUnStuck(                          "Action<CTFBot>::OnUnStuck");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnPostureChanged(                   "Action<CTFBot>::OnPostureChanged");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, int)>                                                 ft_Action_OnAnimationActivityComplete(        "Action<CTFBot>::OnAnimationActivityComplete");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, int)>                                                 ft_Action_OnAnimationActivityInterrupted(     "Action<CTFBot>::OnAnimationActivityInterrupted");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, animevent_t *)>                                       ft_Action_OnAnimationEvent(                   "Action<CTFBot>::OnAnimationEvent");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnIgnite(                           "Action<CTFBot>::OnIgnite");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, const CTakeDamageInfo&)>                              ft_Action_OnInjured(                          "Action<CTFBot>::OnInjured");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, const CTakeDamageInfo&)>                              ft_Action_OnKilled(                           "Action<CTFBot>::OnKilled");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseCombatCharacter *, const CTakeDamageInfo&)>      ft_Action_OnOtherKilled(                      "Action<CTFBot>::OnOtherKilled");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnSight(                            "Action<CTFBot>::OnSight");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnLostSight(                        "Action<CTFBot>::OnLostSight");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *, const Vector&, KeyValues *)>           ft_Action_OnSound(                            "Action<CTFBot>::OnSound");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseCombatCharacter *, const char *, AI_Response *)> ft_Action_OnSpokeConcept(                     "Action<CTFBot>::OnSpokeConcept");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseCombatCharacter *, CBaseCombatWeapon *)>         ft_Action_OnWeaponFired(                      "Action<CTFBot>::OnWeaponFired");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CNavArea *, CNavArea *)>                              ft_Action_OnNavAreaChanged(                   "Action<CTFBot>::OnNavAreaChanged");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnModelChanged(                     "Action<CTFBot>::OnModelChanged");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *, CBaseCombatCharacter *)>               ft_Action_OnPickUp(                           "Action<CTFBot>::OnPickUp");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnDrop(                             "Action<CTFBot>::OnDrop");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseCombatCharacter *, int)>                         ft_Action_OnActorEmoted(                      "Action<CTFBot>::OnActorEmoted");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnCommandAttack(                    "Action<CTFBot>::OnCommandAttack");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, const Vector&, float)>                                ft_Action_OnCommandApproach_vec(              "Action<CTFBot>::OnCommandApproach(vec)");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnCommandApproach_ent(              "Action<CTFBot>::OnCommandApproach(ent)");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *, float)>                                ft_Action_OnCommandRetreat(                   "Action<CTFBot>::OnCommandRetreat");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, float)>                                               ft_Action_OnCommandPause(                     "Action<CTFBot>::OnCommandPause");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnCommandResume(                    "Action<CTFBot>::OnCommandResume");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, const char *)>                                        ft_Action_OnCommandString(                    "Action<CTFBot>::OnCommandString");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnShoved(                           "Action<CTFBot>::OnShoved");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, CBaseEntity *)>                                       ft_Action_OnBlinded(                          "Action<CTFBot>::OnBlinded");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, int)>                                                 ft_Action_OnTerritoryContested(               "Action<CTFBot>::OnTerritoryContested");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, int)>                                                 ft_Action_OnTerritoryCaptured(                "Action<CTFBot>::OnTerritoryCaptured");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor, int)>                                                 ft_Action_OnTerritoryLost(                    "Action<CTFBot>::OnTerritoryLost");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnWin(                              "Action<CTFBot>::OnWin");
static FuncThunk<EDR              (*)(Action<CTFBot> *, CTFBot *actor)>                                                      ft_Action_OnLose(                             "Action<CTFBot>::OnLose");
static FuncThunk<bool             (*)(const Action<CTFBot> *, const INextBot *)>                                             ft_Action_IsAbleToBlockMovementOf(            "Action<CTFBot>::IsAbleToBlockMovementOf");

static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressFireButton(         "CTFBot::PressFireButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseFireButton(       "NextBotPlayer<CTFPlayer>::ReleaseFireButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressAltFireButton(      "CTFBot::PressAltFireButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseAltFireButton(    "NextBotPlayer<CTFPlayer>::ReleaseAltFireButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressMeleeButton(        "NextBotPlayer<CTFPlayer>::PressMeleeButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseMeleeButton(      "NextBotPlayer<CTFPlayer>::ReleaseMeleeButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressSpecialFireButton(  "CTFBot::PressSpecialFireButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseSpecialFireButton("NextBotPlayer<CTFPlayer>::ReleaseSpecialFireButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressUseButton(          "NextBotPlayer<CTFPlayer>::PressUseButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseUseButton(        "NextBotPlayer<CTFPlayer>::ReleaseUseButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressReloadButton(       "NextBotPlayer<CTFPlayer>::PressReloadButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseReloadButton(     "NextBotPlayer<CTFPlayer>::ReleaseReloadButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressForwardButton(      "NextBotPlayer<CTFPlayer>::PressForwardButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseForwardButton(    "NextBotPlayer<CTFPlayer>::ReleaseForwardButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressBackwardButton(     "NextBotPlayer<CTFPlayer>::PressBackwardButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseBackwardButton(   "NextBotPlayer<CTFPlayer>::ReleaseBackwardButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressLeftButton(         "NextBotPlayer<CTFPlayer>::PressLeftButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseLeftButton(       "NextBotPlayer<CTFPlayer>::ReleaseLeftButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressRightButton(        "NextBotPlayer<CTFPlayer>::PressRightButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseRightButton(      "NextBotPlayer<CTFPlayer>::ReleaseRightButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressJumpButton(         "NextBotPlayer<CTFPlayer>::PressJumpButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseJumpButton(       "NextBotPlayer<CTFPlayer>::ReleaseJumpButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressCrouchButton(       "NextBotPlayer<CTFPlayer>::PressCrouchButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseCrouchButton(     "NextBotPlayer<CTFPlayer>::ReleaseCrouchButton");
static FuncThunk<void (*)(CTFBot *, float)>        ft_CTFBot_PressWalkButton(         "NextBotPlayer<CTFPlayer>::PressWalkButton");
static FuncThunk<void (*)(CTFBot *)>               ft_CTFBot_ReleaseWalkButton(       "NextBotPlayer<CTFPlayer>::ReleaseWalkButton");
static FuncThunk<void (*)(CTFBot *, float, float)> ft_CTFBot_SetButtonScale(          "NextBotPlayer<CTFPlayer>::SetButtonScale");

static FuncThunk<float (*)(const CTFBotPathCost *, CNavArea *, CNavArea *, const CNavLadder *, const CFuncElevator *, float)> ft_CTFBotPathCost_op_func("CTFBotPathCost::operator()");


INextBotEventResponder *INextBotEventResponder::FirstContainedResponder() const                               { return (*ft_INextBotEventResponder_FirstContainedResponder)       (this);                    }
INextBotEventResponder *INextBotEventResponder::NextContainedResponder(INextBotEventResponder *prev) const    { return (*ft_INextBotEventResponder_NextContainedResponder)        (this, prev);              }
void INextBotEventResponder::OnLeaveGround(CBaseEntity *ent)                                                  {        (*ft_INextBotEventResponder_OnLeaveGround)                 (this, ent);               }
void INextBotEventResponder::OnLandOnGround(CBaseEntity *ent)                                                 {        (*ft_INextBotEventResponder_OnLandOnGround)                (this, ent);               }
void INextBotEventResponder::OnContact(CBaseEntity *ent, CGameTrace *trace)                                   {        (*ft_INextBotEventResponder_OnContact)                     (this, ent, trace);        }
void INextBotEventResponder::OnMoveToSuccess(const Path *path)                                                {        (*ft_INextBotEventResponder_OnMoveToSuccess)               (this, path);              }
void INextBotEventResponder::OnMoveToFailure(const Path *path, MoveToFailureType fail)                        {        (*ft_INextBotEventResponder_OnMoveToFailure)               (this, path, fail);        }
void INextBotEventResponder::OnStuck()                                                                        {        (*ft_INextBotEventResponder_OnStuck)                       (this);                    }
void INextBotEventResponder::OnUnStuck()                                                                      {        (*ft_INextBotEventResponder_OnUnStuck)                     (this);                    }
void INextBotEventResponder::OnPostureChanged()                                                               {        (*ft_INextBotEventResponder_OnPostureChanged)              (this);                    }
void INextBotEventResponder::OnAnimationActivityComplete(int i1)                                              {        (*ft_INextBotEventResponder_OnAnimationActivityComplete)   (this, i1);                }
void INextBotEventResponder::OnAnimationActivityInterrupted(int i1)                                           {        (*ft_INextBotEventResponder_OnAnimationActivityInterrupted)(this, i1);                }
void INextBotEventResponder::OnAnimationEvent(animevent_t *a1)                                                {        (*ft_INextBotEventResponder_OnAnimationEvent)              (this, a1);                }
void INextBotEventResponder::OnIgnite()                                                                       {        (*ft_INextBotEventResponder_OnIgnite)                      (this);                    }
void INextBotEventResponder::OnInjured(const CTakeDamageInfo& info)                                           {        (*ft_INextBotEventResponder_OnInjured)                     (this, info);              }
void INextBotEventResponder::OnKilled(const CTakeDamageInfo& info)                                            {        (*ft_INextBotEventResponder_OnKilled)                      (this, info);              }
void INextBotEventResponder::OnOtherKilled(CBaseCombatCharacter *who, const CTakeDamageInfo& info)            {        (*ft_INextBotEventResponder_OnOtherKilled)                 (this, who, info);         }
void INextBotEventResponder::OnSight(CBaseEntity *ent)                                                        {        (*ft_INextBotEventResponder_OnSight)                       (this, ent);               }
void INextBotEventResponder::OnLostSight(CBaseEntity *ent)                                                    {        (*ft_INextBotEventResponder_OnLostSight)                   (this, ent);               }
void INextBotEventResponder::OnSound(CBaseEntity *ent, const Vector& v1, KeyValues *kv)                       {        (*ft_INextBotEventResponder_OnSound)                       (this, ent, v1, kv);       }
void INextBotEventResponder::OnSpokeConcept(CBaseCombatCharacter *who, const char *s1, AI_Response *response) {        (*ft_INextBotEventResponder_OnSpokeConcept)                (this, who, s1, response); }
void INextBotEventResponder::OnWeaponFired(CBaseCombatCharacter *who, CBaseCombatWeapon *weapon)              {        (*ft_INextBotEventResponder_OnWeaponFired)                 (this, who, weapon);       }
void INextBotEventResponder::OnNavAreaChanged(CNavArea *area1, CNavArea *area2)                               {        (*ft_INextBotEventResponder_OnNavAreaChanged)              (this, area1, area2);      }
void INextBotEventResponder::OnModelChanged()                                                                 {        (*ft_INextBotEventResponder_OnModelChanged)                (this);                    }
void INextBotEventResponder::OnPickUp(CBaseEntity *ent, CBaseCombatCharacter *who)                            {        (*ft_INextBotEventResponder_OnPickUp)                      (this, ent, who);          }
void INextBotEventResponder::OnDrop(CBaseEntity *ent)                                                         {        (*ft_INextBotEventResponder_OnDrop)                        (this, ent);               }
void INextBotEventResponder::OnActorEmoted(CBaseCombatCharacter *who, int concept)                            {        (*ft_INextBotEventResponder_OnActorEmoted)                 (this, who, concept);      }
void INextBotEventResponder::OnCommandAttack(CBaseEntity *ent)                                                {        (*ft_INextBotEventResponder_OnCommandAttack)               (this, ent);               }
void INextBotEventResponder::OnCommandApproach(const Vector& v1, float f1)                                    {        (*ft_INextBotEventResponder_OnCommandApproach_vec)         (this, v1, f1);            }
void INextBotEventResponder::OnCommandApproach(CBaseEntity *ent)                                              {        (*ft_INextBotEventResponder_OnCommandApproach_ent)         (this, ent);               }
void INextBotEventResponder::OnCommandRetreat(CBaseEntity *ent, float f1)                                     {        (*ft_INextBotEventResponder_OnCommandRetreat)              (this, ent, f1);           }
void INextBotEventResponder::OnCommandPause(float f1)                                                         {        (*ft_INextBotEventResponder_OnCommandPause)                (this, f1);                }
void INextBotEventResponder::OnCommandResume()                                                                {        (*ft_INextBotEventResponder_OnCommandResume)               (this);                    }
void INextBotEventResponder::OnCommandString(const char *cmd)                                                 {        (*ft_INextBotEventResponder_OnCommandString)               (this, cmd);               }
void INextBotEventResponder::OnShoved(CBaseEntity *ent)                                                       {        (*ft_INextBotEventResponder_OnShoved)                      (this, ent);               }
void INextBotEventResponder::OnBlinded(CBaseEntity *ent)                                                      {        (*ft_INextBotEventResponder_OnBlinded)                     (this, ent);               }
void INextBotEventResponder::OnTerritoryContested(int i1)                                                     {        (*ft_INextBotEventResponder_OnTerritoryContested)          (this, i1);                }
void INextBotEventResponder::OnTerritoryCaptured(int i1)                                                      {        (*ft_INextBotEventResponder_OnTerritoryCaptured)           (this, i1);                }
void INextBotEventResponder::OnTerritoryLost(int i1)                                                          {        (*ft_INextBotEventResponder_OnTerritoryLost)               (this, i1);                }
void INextBotEventResponder::OnWin()                                                                          {        (*ft_INextBotEventResponder_OnWin)                         (this);                    }
void INextBotEventResponder::OnLose()                                                                         {        (*ft_INextBotEventResponder_OnLose)                        (this);                    }

QueryResponse IContextualQuery::ShouldPickUp(const INextBot *nextbot, CBaseEntity *it) const                                                                                               { return (*ft_IContextualQuery_ShouldPickUp)             (this, nextbot, it);                     }
QueryResponse IContextualQuery::ShouldHurry(const INextBot *nextbot) const                                                                                                                 { return (*ft_IContextualQuery_ShouldHurry)              (this, nextbot);                         }
QueryResponse IContextualQuery::ShouldRetreat(const INextBot *nextbot) const                                                                                                               { return (*ft_IContextualQuery_ShouldRetreat)            (this, nextbot);                         }
QueryResponse IContextualQuery::ShouldAttack(const INextBot *nextbot, const CKnownEntity *threat) const                                                                                    { return (*ft_IContextualQuery_ShouldAttack)             (this, nextbot, threat);                 }
QueryResponse IContextualQuery::IsHindrance(const INextBot *nextbot, CBaseEntity *it) const                                                                                                { return (*ft_IContextualQuery_IsHindrance)              (this, nextbot, it);                     }
Vector IContextualQuery::SelectTargetPoint(const INextBot *nextbot, const CBaseCombatCharacter *them) const                                                                                { return (*ft_IContextualQuery_SelectTargetPoint)        (this, nextbot, them);                   }
QueryResponse IContextualQuery::IsPositionAllowed(const INextBot *nextbot, const Vector& v1) const                                                                                         { return (*ft_IContextualQuery_IsPositionAllowed)        (this, nextbot, v1);                     }
const CKnownEntity *IContextualQuery::SelectMoreDangerousThreat(const INextBot *nextbot, const CBaseCombatCharacter *them, const CKnownEntity *threat1, const CKnownEntity *threat2) const { return (*ft_IContextualQuery_SelectMoreDangerousThreat)(this, nextbot, them, threat1, threat2); }

template<> INextBotEventResponder *Action<CTFBot>::FirstContainedResponder() const                                                                    { return (*ft_Action_INER_FirstContainedResponder)       (this);                           }
template<> INextBotEventResponder *Action<CTFBot>::NextContainedResponder(INextBotEventResponder *prev) const                                         { return (*ft_Action_INER_NextContainedResponder)        (this, prev);                     }
template<> void Action<CTFBot>::OnLeaveGround(CBaseEntity *ent)                                                                                       {        (*ft_Action_INER_OnLeaveGround)                 (this, ent);                      }
template<> void Action<CTFBot>::OnLandOnGround(CBaseEntity *ent)                                                                                      {        (*ft_Action_INER_OnLandOnGround)                (this, ent);                      }
template<> void Action<CTFBot>::OnContact(CBaseEntity *ent, CGameTrace *trace)                                                                        {        (*ft_Action_INER_OnContact)                     (this, ent, trace);               }
template<> void Action<CTFBot>::OnMoveToSuccess(const Path *path)                                                                                     {        (*ft_Action_INER_OnMoveToSuccess)               (this, path);                     }
template<> void Action<CTFBot>::OnMoveToFailure(const Path *path, MoveToFailureType fail)                                                             {        (*ft_Action_INER_OnMoveToFailure)               (this, path, fail);               }
template<> void Action<CTFBot>::OnStuck()                                                                                                             {        (*ft_Action_INER_OnStuck)                       (this);                           }
template<> void Action<CTFBot>::OnUnStuck()                                                                                                           {        (*ft_Action_INER_OnUnStuck)                     (this);                           }
template<> void Action<CTFBot>::OnPostureChanged()                                                                                                    {        (*ft_Action_INER_OnPostureChanged)              (this);                           }
template<> void Action<CTFBot>::OnAnimationActivityComplete(int i1)                                                                                   {        (*ft_Action_INER_OnAnimationActivityComplete)   (this, i1);                       }
template<> void Action<CTFBot>::OnAnimationActivityInterrupted(int i1)                                                                                {        (*ft_Action_INER_OnAnimationActivityInterrupted)(this, i1);                       }
template<> void Action<CTFBot>::OnAnimationEvent(animevent_t *a1)                                                                                     {        (*ft_Action_INER_OnAnimationEvent)              (this, a1);                       }
template<> void Action<CTFBot>::OnIgnite()                                                                                                            {        (*ft_Action_INER_OnIgnite)                      (this);                           }
template<> void Action<CTFBot>::OnInjured(const CTakeDamageInfo& info)                                                                                {        (*ft_Action_INER_OnInjured)                     (this, info);                     }
template<> void Action<CTFBot>::OnKilled(const CTakeDamageInfo& info)                                                                                 {        (*ft_Action_INER_OnKilled)                      (this, info);                     }
template<> void Action<CTFBot>::OnOtherKilled(CBaseCombatCharacter *who, const CTakeDamageInfo& info)                                                 {        (*ft_Action_INER_OnOtherKilled)                 (this, who, info);                }
template<> void Action<CTFBot>::OnSight(CBaseEntity *ent)                                                                                             {        (*ft_Action_INER_OnSight)                       (this, ent);                      }
template<> void Action<CTFBot>::OnLostSight(CBaseEntity *ent)                                                                                         {        (*ft_Action_INER_OnLostSight)                   (this, ent);                      }
template<> void Action<CTFBot>::OnSound(CBaseEntity *ent, const Vector& v1, KeyValues *kv)                                                            {        (*ft_Action_INER_OnSound)                       (this, ent, v1, kv);              }
template<> void Action<CTFBot>::OnSpokeConcept(CBaseCombatCharacter *who, const char *s1, AI_Response *response)                                      {        (*ft_Action_INER_OnSpokeConcept)                (this, who, s1, response);        }
template<> void Action<CTFBot>::OnWeaponFired(CBaseCombatCharacter *who, CBaseCombatWeapon *weapon)                                                   {        (*ft_Action_INER_OnWeaponFired)                 (this, who, weapon);              }
template<> void Action<CTFBot>::OnNavAreaChanged(CNavArea *area1, CNavArea *area2)                                                                    {        (*ft_Action_INER_OnNavAreaChanged)              (this, area1, area2);             }
template<> void Action<CTFBot>::OnModelChanged()                                                                                                      {        (*ft_Action_INER_OnModelChanged)                (this);                           }
template<> void Action<CTFBot>::OnPickUp(CBaseEntity *ent, CBaseCombatCharacter *who)                                                                 {        (*ft_Action_INER_OnPickUp)                      (this, ent, who);                 }
template<> void Action<CTFBot>::OnDrop(CBaseEntity *ent)                                                                                              {        (*ft_Action_INER_OnDrop)                        (this, ent);                      }
template<> void Action<CTFBot>::OnActorEmoted(CBaseCombatCharacter *who, int concept)                                                                 {        (*ft_Action_INER_OnActorEmoted)                 (this, who, concept);             }
template<> void Action<CTFBot>::OnCommandAttack(CBaseEntity *ent)                                                                                     {        (*ft_Action_INER_OnCommandAttack)               (this, ent);                      }
template<> void Action<CTFBot>::OnCommandApproach(const Vector& v1, float f1)                                                                         {        (*ft_Action_INER_OnCommandApproach_vec)         (this, v1, f1);                   }
template<> void Action<CTFBot>::OnCommandApproach(CBaseEntity *ent)                                                                                   {        (*ft_Action_INER_OnCommandApproach_ent)         (this, ent);                      }
template<> void Action<CTFBot>::OnCommandRetreat(CBaseEntity *ent, float f1)                                                                          {        (*ft_Action_INER_OnCommandRetreat)              (this, ent, f1);                  }
template<> void Action<CTFBot>::OnCommandPause(float f1)                                                                                              {        (*ft_Action_INER_OnCommandPause)                (this, f1);                       }
template<> void Action<CTFBot>::OnCommandResume()                                                                                                     {        (*ft_Action_INER_OnCommandResume)               (this);                           }
template<> void Action<CTFBot>::OnCommandString(const char *cmd)                                                                                      {        (*ft_Action_INER_OnCommandString)               (this, cmd);                      }
template<> void Action<CTFBot>::OnShoved(CBaseEntity *ent)                                                                                            {        (*ft_Action_INER_OnShoved)                      (this, ent);                      }
template<> void Action<CTFBot>::OnBlinded(CBaseEntity *ent)                                                                                           {        (*ft_Action_INER_OnBlinded)                     (this, ent);                      }
template<> void Action<CTFBot>::OnTerritoryContested(int i1)                                                                                          {        (*ft_Action_INER_OnTerritoryContested)          (this, i1);                       }
template<> void Action<CTFBot>::OnTerritoryCaptured(int i1)                                                                                           {        (*ft_Action_INER_OnTerritoryCaptured)           (this, i1);                       }
template<> void Action<CTFBot>::OnTerritoryLost(int i1)                                                                                               {        (*ft_Action_INER_OnTerritoryLost)               (this, i1);                       }
template<> void Action<CTFBot>::OnWin()                                                                                                               {        (*ft_Action_INER_OnWin)                         (this);                           }
template<> void Action<CTFBot>::OnLose()                                                                                                              {        (*ft_Action_INER_OnLose)                        (this);                           }
template<> bool Action<CTFBot>::IsNamed(const char *name) const                                                                                       { return (*ft_Action_IsNamed)                            (this, name);                     }
template<> char *Action<CTFBot>::GetFullName() const                                                                                                  { return (*ft_Action_GetFullName)                        (this);                           }
template<> ActionResult<CTFBot> Action<CTFBot>::OnStart(CTFBot *actor, Action<CTFBot> *action)                                                        { return (*ft_Action_OnStart)                            (this, actor, action);            }
template<> ActionResult<CTFBot> Action<CTFBot>::Update(CTFBot *actor, float dt)                                                                       { return (*ft_Action_Update)                             (this, actor, dt);                }
template<> void Action<CTFBot>::OnEnd(CTFBot *actor, Action<CTFBot> *action)                                                                          {        (*ft_Action_OnEnd)                              (this, actor, action);            }
template<> ActionResult<CTFBot> Action<CTFBot>::OnSuspend(CTFBot *actor, Action<CTFBot> *action)                                                      { return (*ft_Action_OnSuspend)                          (this, actor, action);            }
template<> ActionResult<CTFBot> Action<CTFBot>::OnResume(CTFBot *actor, Action<CTFBot> *action)                                                       { return (*ft_Action_OnResume)                           (this, actor, action);            }
template<> Action<CTFBot> *Action<CTFBot>::InitialContainedAction(CTFBot *actor)                                                                      { return (*ft_Action_InitialContainedAction)             (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnLeaveGround(CTFBot *actor, CBaseEntity *ent)                                                  { return (*ft_Action_OnLeaveGround)                      (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnLandOnGround(CTFBot *actor, CBaseEntity *ent)                                                 { return (*ft_Action_OnLandOnGround)                     (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnContact(CTFBot *actor, CBaseEntity *ent, CGameTrace *trace)                                   { return (*ft_Action_OnContact)                          (this, actor, ent, trace);        }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnMoveToSuccess(CTFBot *actor, const Path *path)                                                { return (*ft_Action_OnMoveToSuccess)                    (this, actor, path);              }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnMoveToFailure(CTFBot *actor, const Path *path, MoveToFailureType fail)                        { return (*ft_Action_OnMoveToFailure)                    (this, actor, path, fail);        }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnStuck(CTFBot *actor)                                                                          { return (*ft_Action_OnStuck)                            (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnUnStuck(CTFBot *actor)                                                                        { return (*ft_Action_OnUnStuck)                          (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnPostureChanged(CTFBot *actor)                                                                 { return (*ft_Action_OnPostureChanged)                   (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnAnimationActivityComplete(CTFBot *actor, int i1)                                              { return (*ft_Action_OnAnimationActivityComplete)        (this, actor, i1);                }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnAnimationActivityInterrupted(CTFBot *actor, int i1)                                           { return (*ft_Action_OnAnimationActivityInterrupted)     (this, actor, i1);                }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnAnimationEvent(CTFBot *actor, animevent_t *a1)                                                { return (*ft_Action_OnAnimationEvent)                   (this, actor, a1);                }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnIgnite(CTFBot *actor)                                                                         { return (*ft_Action_OnIgnite)                           (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnInjured(CTFBot *actor, const CTakeDamageInfo& info)                                           { return (*ft_Action_OnInjured)                          (this, actor, info);              }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnKilled(CTFBot *actor, const CTakeDamageInfo& info)                                            { return (*ft_Action_OnKilled)                           (this, actor, info);              }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnOtherKilled(CTFBot *actor, CBaseCombatCharacter *who, const CTakeDamageInfo& info)            { return (*ft_Action_OnOtherKilled)                      (this, actor, who, info);         }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnSight(CTFBot *actor, CBaseEntity *ent)                                                        { return (*ft_Action_OnSight)                            (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnLostSight(CTFBot *actor, CBaseEntity *ent)                                                    { return (*ft_Action_OnLostSight)                        (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnSound(CTFBot *actor, CBaseEntity *ent, const Vector& v1, KeyValues *kv)                       { return (*ft_Action_OnSound)                            (this, actor, ent, v1, kv);       }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnSpokeConcept(CTFBot *actor, CBaseCombatCharacter *who, const char *s1, AI_Response *response) { return (*ft_Action_OnSpokeConcept)                     (this, actor, who, s1, response); }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnWeaponFired(CTFBot *actor, CBaseCombatCharacter *who, CBaseCombatWeapon *weapon)              { return (*ft_Action_OnWeaponFired)                      (this, actor, who, weapon);       }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnNavAreaChanged(CTFBot *actor, CNavArea *area1, CNavArea *area2)                               { return (*ft_Action_OnNavAreaChanged)                   (this, actor, area1, area2);      }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnModelChanged(CTFBot *actor)                                                                   { return (*ft_Action_OnModelChanged)                     (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnPickUp(CTFBot *actor, CBaseEntity *ent, CBaseCombatCharacter *who)                            { return (*ft_Action_OnPickUp)                           (this, actor, ent, who);          }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnDrop(CTFBot *actor, CBaseEntity *ent)                                                         { return (*ft_Action_OnDrop)                             (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnActorEmoted(CTFBot *actor, CBaseCombatCharacter *who, int i1)                                 { return (*ft_Action_OnActorEmoted)                      (this, actor, who, i1);           }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnCommandAttack(CTFBot *actor, CBaseEntity *ent)                                                { return (*ft_Action_OnCommandAttack)                    (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnCommandApproach(CTFBot *actor, const Vector& v1, float f1)                                    { return (*ft_Action_OnCommandApproach_vec)              (this, actor, v1, f1);            }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnCommandApproach(CTFBot *actor, CBaseEntity *ent)                                              { return (*ft_Action_OnCommandApproach_ent)              (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnCommandRetreat(CTFBot *actor, CBaseEntity *ent, float f1)                                     { return (*ft_Action_OnCommandRetreat)                   (this, actor, ent, f1);           }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnCommandPause(CTFBot *actor, float f1)                                                         { return (*ft_Action_OnCommandPause)                     (this, actor, f1);                }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnCommandResume(CTFBot *actor)                                                                  { return (*ft_Action_OnCommandResume)                    (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnCommandString(CTFBot *actor, const char *cmd)                                                 { return (*ft_Action_OnCommandString)                    (this, actor, cmd);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnShoved(CTFBot *actor, CBaseEntity *ent)                                                       { return (*ft_Action_OnShoved)                           (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnBlinded(CTFBot *actor, CBaseEntity *ent)                                                      { return (*ft_Action_OnBlinded)                          (this, actor, ent);               }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnTerritoryContested(CTFBot *actor, int i1)                                                     { return (*ft_Action_OnTerritoryContested)               (this, actor, i1);                }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnTerritoryCaptured(CTFBot *actor, int i1)                                                      { return (*ft_Action_OnTerritoryCaptured)                (this, actor, i1);                }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnTerritoryLost(CTFBot *actor, int i1)                                                          { return (*ft_Action_OnTerritoryLost)                    (this, actor, i1);                }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnWin(CTFBot *actor)                                                                            { return (*ft_Action_OnWin)                              (this, actor);                    }
template<> EventDesiredResult<CTFBot> Action<CTFBot>::OnLose(CTFBot *actor)                                                                           { return (*ft_Action_OnLose)                             (this, actor);                    }
template<> bool Action<CTFBot>::IsAbleToBlockMovementOf(const INextBot *nextbot) const                                                                { return (*ft_Action_IsAbleToBlockMovementOf)            (this, nextbot);                  }

void CTFBot::PressFireButton(float duration)           { (*ft_CTFBot_PressFireButton)         (this, duration);      }
void CTFBot::ReleaseFireButton()                       { (*ft_CTFBot_ReleaseFireButton)       (this);                }
void CTFBot::PressAltFireButton(float duration)        { (*ft_CTFBot_PressAltFireButton)      (this, duration);      }
void CTFBot::ReleaseAltFireButton()                    { (*ft_CTFBot_ReleaseAltFireButton)    (this);                }
void CTFBot::PressMeleeButton(float duration)          { (*ft_CTFBot_PressMeleeButton)        (this, duration);      }
void CTFBot::ReleaseMeleeButton()                      { (*ft_CTFBot_ReleaseMeleeButton)      (this);                }
void CTFBot::PressSpecialFireButton(float duration)    { (*ft_CTFBot_PressSpecialFireButton)  (this, duration);      }
void CTFBot::ReleaseSpecialFireButton()                { (*ft_CTFBot_ReleaseSpecialFireButton)(this);                }
void CTFBot::PressUseButton(float duration)            { (*ft_CTFBot_PressUseButton)          (this, duration);      }
void CTFBot::ReleaseUseButton()                        { (*ft_CTFBot_ReleaseUseButton)        (this);                }
void CTFBot::PressReloadButton(float duration)         { (*ft_CTFBot_PressReloadButton)       (this, duration);      }
void CTFBot::ReleaseReloadButton()                     { (*ft_CTFBot_ReleaseReloadButton)     (this);                }
void CTFBot::PressForwardButton(float duration)        { (*ft_CTFBot_PressForwardButton)      (this, duration);      }
void CTFBot::ReleaseForwardButton()                    { (*ft_CTFBot_ReleaseForwardButton)    (this);                }
void CTFBot::PressBackwardButton(float duration)       { (*ft_CTFBot_PressBackwardButton)     (this, duration);      }
void CTFBot::ReleaseBackwardButton()                   { (*ft_CTFBot_ReleaseBackwardButton)   (this);                }
void CTFBot::PressLeftButton(float duration)           { (*ft_CTFBot_PressLeftButton)         (this, duration);      }
void CTFBot::ReleaseLeftButton()                       { (*ft_CTFBot_ReleaseLeftButton)       (this);                }
void CTFBot::PressRightButton(float duration)          { (*ft_CTFBot_PressRightButton)        (this, duration);      }
void CTFBot::ReleaseRightButton()                      { (*ft_CTFBot_ReleaseRightButton)      (this);                }
void CTFBot::PressJumpButton(float duration)           { (*ft_CTFBot_PressJumpButton)         (this, duration);      }
void CTFBot::ReleaseJumpButton()                       { (*ft_CTFBot_ReleaseJumpButton)       (this);                }
void CTFBot::PressCrouchButton(float duration)         { (*ft_CTFBot_PressCrouchButton)       (this, duration);      }
void CTFBot::ReleaseCrouchButton()                     { (*ft_CTFBot_ReleaseCrouchButton)     (this);                }
void CTFBot::PressWalkButton(float duration)           { (*ft_CTFBot_PressWalkButton)         (this, duration);      }
void CTFBot::ReleaseWalkButton()                       { (*ft_CTFBot_ReleaseWalkButton)       (this);                }
void CTFBot::SetButtonScale(float forward, float side) { (*ft_CTFBot_SetButtonScale)          (this, forward, side); }

float CTFBotPathCost::operator()(CNavArea *area1, CNavArea *area2, const CNavLadder *ladder, const CFuncElevator *elevator, float f1) const { return (*ft_CTFBotPathCost_op_func)(this, area1, area2, ladder, elevator, f1); }
