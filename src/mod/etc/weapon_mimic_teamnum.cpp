#include "mod.h"
#include "stub/baseentity.h"
#include "stub/extraentitydata.h"
#include "stub/projectiles.h"
#include "stub/tfentities.h"
#include "stub/misc.h"
#include "util/misc.h"
#include "PlayerState.h"
#include "mod/etc/mapentity_additions.h"
#include "mod/common/weapon_shoot.h"
#include "util/iterate.h"


namespace Mod::Etc::Weapon_Mimic_Teamnum
{
    CBaseEntity *projectile = nullptr;
    RefCount rc_CTFPointWeaponMimic_Fire;
	CBaseEntity *scorer = nullptr;
	bool grenade = false;
	bool do_crits = false;
	CBaseEntity *mimicFire = nullptr;

	class MimicModule : public EntityModule
	{
	public:
		MimicModule(CBaseEntity *ent) : EntityModule(ent) {}

		CHandle<CTFWeaponBaseGun> weapon = nullptr;
		std::string lastWeaponName;
		std::unordered_map<std::string, std::string> attribs;
	};

	THINK_FUNC_DECL(MimicFire)
	{
		auto *mimic = reinterpret_cast<CTFPointWeaponMimic *>(this);
		int times = mimic->GetCustomVariableFloat<"shootmultcount">();
		for (int i = 0; i < times; i++) {
			mimic->Fire();
		}
		this->SetNextThink(gpGlobals->curtime + mimic->GetCustomVariableFloat<"firetime">(), "MimicFire");
	}

	Mod::Etc::Mapentity_Additions::ClassnameFilter mimic_filter("tf_point_weapon_mimic", {
		{"StartFiring"sv, true, [](CBaseEntity *ent, const char *szInputName, CBaseEntity *pActivator, CBaseEntity *pCaller, variant_t &Value){
            auto *mimic = static_cast<CTFPointWeaponMimic *>(ent);
			variant_t times;
			times.SetInt(Max(1, Value.Int()));
			mimic->SetCustomVariable("shootmultcount", times);
			for (int i = 0; i < times.Int(); i++) {
				mimic->Fire();
			}
			THINK_FUNC_SET(mimic, MimicFire, gpGlobals->curtime + mimic->GetCustomVariableFloat<"firetime">());
        }},
		{"StopFiring"sv, true, [](CBaseEntity *ent, const char *szInputName, CBaseEntity *pActivator, CBaseEntity *pCaller, variant_t &Value){
            ent->SetNextThink(-1, "MimicFire");
        }},
        {"AddWeaponAttribute"sv, true, [](CBaseEntity *ent, const char *szInputName, CBaseEntity *pActivator, CBaseEntity *pCaller, variant_t &Value){
            auto *mimic = static_cast<CTFPointWeaponMimic *>(ent);
			char param_tokenized[512];
			V_strncpy(param_tokenized, Value.String(), sizeof(param_tokenized));
			
			char *attr = strtok(param_tokenized,"|");
			char *value = strtok(NULL,"|");
			if (attr != nullptr && value != nullptr) {
				auto mod = mimic->GetOrCreateEntityModule<MimicModule>("weaponmimic");
				CEconItemAttributeDefinition *def = GetItemSchema()->GetAttributeDefinitionByName(attr);
				if (def != nullptr) {
					mod->attribs[attr] = value;
					if (mod->weapon != nullptr && mod->weapon->GetItem() != nullptr) {
						mod->weapon->GetItem()->GetAttributeList().AddStringAttribute(def, value);
					}
				}
			}
        }},
		{"RemoveWeaponAttribute"sv, true, [](CBaseEntity *ent, const char *szInputName, CBaseEntity *pActivator, CBaseEntity *pCaller, variant_t &Value){
            auto *mimic = static_cast<CTFPointWeaponMimic *>(ent);
			auto mod = mimic->GetOrCreateEntityModule<MimicModule>("weaponmimic");
			CEconItemAttributeDefinition *def = GetItemSchema()->GetAttributeDefinitionByName(szInputName);
			mod->attribs.erase(szInputName);
			if (def != nullptr) {
				if (mod->weapon != nullptr && mod->weapon->GetItem() != nullptr) {
					mod->weapon->GetItem()->GetAttributeList().RemoveAttribute(def);
				}
			}
        }}
	});
	
    VHOOK_DECL(void, CBaseEntity_ModifyFireBulletsDamage, CTakeDamageInfo* dmgInfo)
	{
		if (do_crits) {
			dmgInfo->SetDamageType(dmgInfo->GetDamageType() | DMG_CRITICAL);
		}
		if (mimicFire != nullptr) {
			variant_t variant;
			variant.SetInt(-1);
			mimicFire->GetCustomVariableVariant<"dmgtype">(variant);
			variant.Convert(FIELD_INTEGER);
			int dmgtype = variant.Int();
			if (dmgtype != -1) {
				dmgInfo->SetDamageType(dmgtype);
			}
		}
        VHOOK_CALL(CBaseEntity_ModifyFireBulletsDamage)(dmgInfo);
	}

	CVirtualHook bulletDamageHook(TypeName<CBaseEntity>(), "CBaseEntity::ModifyFireBulletsDamage", GET_VHOOK_CALLBACK(CBaseEntity_ModifyFireBulletsDamage), GET_VHOOK_INNERPTR(CBaseEntity_ModifyFireBulletsDamage));
	CTFWeaponBase *shooting_weapon = nullptr;
	
	void MimicFire(CTFPointWeaponMimic *mimic)
	{
        SCOPED_INCREMENT(rc_CTFPointWeaponMimic_Fire);
		mimicFire = mimic;
        projectile = nullptr;
		grenade = false;
		
		variant_t variant;
		int spawnflags = mimic->m_spawnflags;
		if (mimic->GetOwnerEntity() != nullptr && mimic->GetOwnerEntity()->IsPlayer()) {
			scorer = mimic->GetOwnerEntity();
		}
		
		const char *weaponName = mimic->GetCustomVariable<"weaponname">();
		if (weaponName != nullptr) {
			auto mod = mimic->GetOrCreateEntityModule<MimicModule>("weaponmimic");
			CTFWeaponBaseGun *weapon = mod->weapon;
			// Create new weapon if not created yet
			if (weapon == nullptr || mod->lastWeaponName != weaponName) {
				if (weapon != nullptr) {
					weapon->Remove();
				}
				auto item = CreateItemByName(nullptr, weaponName);
				weapon = rtti_cast<CTFWeaponBaseGun *>(item);
				if (weapon != nullptr) {
					mod->lastWeaponName = weaponName;
					mod->weapon = weapon;
					for (auto& [attr,value] : mod->attribs) {
						CEconItemAttributeDefinition *def = GetItemSchema()->GetAttributeDefinitionByName(attr.c_str());
						if (def != nullptr) {
							mod->weapon->GetItem()->GetAttributeList().AddStringAttribute(def, value);
						}
					}
					variant_t var;
					var.SetEntity(mimic);
					weapon->SetCustomVariable("mimicent", var);
				}
				else if (item != nullptr) {
					item->Remove();
				}
			}
			if (weapon != nullptr) {
				projectile = Mod::Common::Weapon_Shoot::FireWeapon(mimic, weapon, mimic->GetAbsOrigin(), mimic->GetAbsAngles(), mimic->m_bCrits, false, TF_TEAM_BLUE);
				auto pipebomb = rtti_cast<CTFGrenadePipebombProjectile *>(projectile);
				if (pipebomb != nullptr && (pipebomb->m_iType == 1 || pipebomb->m_iType == 2)) {
					mimic->m_Pipebombs.Get().AddToTail(pipebomb);
				}
			}
		}
		else if (mimic->m_nWeaponType == 4) {
			FireBulletsInfo_t info;

			QAngle vFireAngles = mimic->GetFiringAngles();
			Vector vForward;
			AngleVectors( vFireAngles, &vForward, nullptr, nullptr);

			info.m_vecSrc = mimic->GetAbsOrigin();
			info.m_vecDirShooting = vForward;
			info.m_iTracerFreq = 1;
			info.m_iShots = 1;
			info.m_pAttacker = mimic->GetOwnerEntity();
			if (info.m_pAttacker == nullptr) {
				info.m_pAttacker = mimic;
			}

			info.m_vecSpread = vec3_origin;

			if (mimic->m_flSpeedMax != 0.0f) {
				info.m_flDistance = mimic->m_flSpeedMax;
			}
			else {
				info.m_flDistance = 8192.0f;
			}
			info.m_iAmmoType = 1;//m_iAmmoType;
			info.m_flDamage = mimic->m_flDamage;
			info.m_flDamageForceScale = mimic->m_flSplashRadius;

			// Prevent the mimic from shooting the root parent
			if (mimic->GetCustomVariableFloat<"preventshootparent">(1.0f)) {
				CBaseEntity *rootEntity = mimic;
				while (rootEntity) {
					CBaseEntity *parent = rootEntity->GetMoveParent();
					if (parent == nullptr) {
						break;
					}
					else {
						rootEntity = parent;
					}
				}
				info.m_pAdditionalIgnoreEnt = rootEntity;
			}

			do_crits = mimic->m_bCrits;
			bulletDamageHook.ChangeVTable(*(void ***)mimic);
			bulletDamageHook.DoEnable();
			mimic->FireBullets(info);
			bulletDamageHook.DoDisable();
			do_crits = false;
			projectile = nullptr;
			
		}
		else {
        	mimic->Fire();
		}

        if (projectile != nullptr) {
			// Set models to rockets and arrows
			//if (mimic->m_pzsModelOverride != NULL_STRING && (spawnflags & 2) && (mimic->m_nWeaponType == 0 || mimic->m_nWeaponType == 2)) {
			//	projectile->SetModel(mimic->m_pzsModelOverride);
			//}

			variant_t var;
			var.SetEntity(mimic);
			projectile->SetCustomVariable("mimicent", var);

			if (mimic->GetCustomVariableVariant<"killicon">(var)) {
				projectile->SetCustomVariable("killicon", var);
			}

			if (mimic->GetCustomVariableVariant<"dmgtype">(var)) {
				projectile->SetCustomVariable("dmgtype", var);
			}

            if (mimic->GetTeamNumber() != 0) {
				CBaseAnimating *anim = rtti_cast<CBaseAnimating *>(projectile);
                projectile->SetTeamNumber(mimic->GetTeamNumber());
				if (anim->m_nSkin == 1 && mimic->GetTeamNumber() == 2)
					anim->m_nSkin = 0;
			}
			if (grenade) {
				projectile->SetOwnerEntity(scorer);
			}
			// Fire callback
			variant_t variant;
			variant.SetString(NULL_STRING);
			if (spawnflags & 1) {
                mimic->m_OnUser4->FireOutput(variant, projectile, mimic);
			}
			mimic->FireCustomOutput<"onfire">(projectile, mimic, variant);
			
			// Play sound, Particles
			if (spawnflags & 2) {
				string_t particle = mimic->m_pzsFireParticles;
				string_t sound = mimic->m_pzsFireSound;
				if (sound != NULL_STRING)
					mimic->EmitSound(STRING(sound));
				if (particle != NULL_STRING)
					DispatchParticleEffect(STRING(particle), PATTACH_ABSORIGIN_FOLLOW, mimic, INVALID_PARTICLE_ATTACHMENT, false);
			}
        }
		scorer = nullptr;
		mimicFire = nullptr;
	}

	// CTFPointWeaponMimic::Fire gets inlined, need to detour inputfireonce/inputfiremultiple
    DETOUR_DECL_MEMBER(void, CTFPointWeaponMimic_Fire)
	{
		auto *mimic = reinterpret_cast<CTFPointWeaponMimic *>(this);
		if (!rc_CTFPointWeaponMimic_Fire) {
			MimicFire(mimic);
		}
		else {
			DETOUR_MEMBER_CALL(CTFPointWeaponMimic_Fire)();
		}
	}

	
    DETOUR_DECL_MEMBER(void, CTFPointWeaponMimic_InputFireOnce, inputdata_t& inputdata)
	{
		auto *mimic = reinterpret_cast<CTFPointWeaponMimic *>(this);
		MimicFire(mimic);
	}

	DETOUR_DECL_MEMBER(void, CTFPointWeaponMimic_InputFireMultiple, inputdata_t& inputdata)
	{
		auto *mimic = reinterpret_cast<CTFPointWeaponMimic *>(this);
		int nNumFires = Max( 1, abs(inputdata.value.Int()) );

		while( nNumFires-- ) {
			MimicFire(mimic);
		}
	}

    DETOUR_DECL_STATIC(CBaseEntity *, CTFProjectile_Rocket_Create, CBaseEntity *pLauncher, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, CBaseEntity *pScorer)
	{
		if (scorer != nullptr) {
			pScorer = scorer;
		}
        projectile = DETOUR_STATIC_CALL(CTFProjectile_Rocket_Create)(pLauncher, vecOrigin, vecAngles, pOwner, pScorer);
        return projectile;
	}

    DETOUR_DECL_STATIC(CTFProjectile_Arrow *, CTFProjectile_Arrow_Create, const Vector &vecOrigin, const QAngle &vecAngles, const float fSpeed, const float fGravity, int projectileType, CBaseEntity *pOwner, CBaseEntity *pScorer)
	{
		if (scorer != nullptr) {
			pScorer = scorer;
		}
        projectile = DETOUR_STATIC_CALL(CTFProjectile_Arrow_Create)(vecOrigin, vecAngles, fSpeed, fGravity, projectileType, pOwner, pScorer);
        return projectile;
	}
	
    DETOUR_DECL_STATIC(CBaseEntity *, CBaseEntity_CreateNoSpawn, const char *szName, const Vector& vecOrigin, const QAngle& vecAngles, CBaseEntity *pOwner)
	{
		if (scorer != nullptr) {
			grenade = true;
			pOwner = scorer;
		}
        projectile = DETOUR_STATIC_CALL(CBaseEntity_CreateNoSpawn)(szName, vecOrigin, vecAngles, pOwner);
        return projectile;
	}

	void OnRemove(CTFPointWeaponMimic *mimic)
	{
		auto mod = mimic->GetEntityModule<MimicModule>("weaponmimic");
		if (mod != nullptr && mod->weapon != nullptr) {
			mod->weapon->Remove();
		}

		for (int i = 0; i < mimic->m_Pipebombs->Count(); ++i) {
			auto proj = mimic->m_Pipebombs[i];
			if (proj != nullptr)
			//DevMsg("owner %d %d\n", proj->GetOwnerEntity(), mimic);
			//if (proj->GetOwnerEntity() == mimic)
				proj->Remove();
		}
	}

	DETOUR_DECL_MEMBER(void, CTFPointWeaponMimic_dtor0)
	{
		OnRemove(reinterpret_cast<CTFPointWeaponMimic *>(this));
		
        DETOUR_MEMBER_CALL(CTFPointWeaponMimic_dtor0)();
	}
	
	DETOUR_DECL_MEMBER(void, CTFPointWeaponMimic_dtor2)
	{
		OnRemove(reinterpret_cast<CTFPointWeaponMimic *>(this));
		
        DETOUR_MEMBER_CALL(CTFPointWeaponMimic_dtor2)();
	}

	DETOUR_DECL_MEMBER(void, CTFPointWeaponMimic_Spawn)
	{
        DETOUR_MEMBER_CALL(CTFPointWeaponMimic_Spawn)();
		auto mimic = reinterpret_cast<CTFPointWeaponMimic *>(this);
		
		int spawnflags = mimic->m_spawnflags;
		string_t sound = mimic->m_pzsFireSound;
		string_t particle = mimic->m_pzsFireParticles;
		if (spawnflags & 2) {
			if (sound != NULL_STRING) {
				if (!enginesound->PrecacheSound(STRING(sound), true))
					CBaseEntity::PrecacheScriptSound(STRING(sound));
			}

			if (particle != NULL_STRING)
				PrecacheParticleSystem(STRING(particle));
		}
		//if ((spawnflags & 2) || mimic->m_nWeaponType == 4) {
			mimic->AddEFlags(EFL_FORCE_CHECK_TRANSMIT);
		//}
	}

	DETOUR_DECL_MEMBER(const char *, CTFGameRules_GetKillingWeaponName, const CTakeDamageInfo &info, CTFPlayer *pVictim, int *iWeaponID)
	{
		
		if (mimicFire != nullptr) {
			auto killIcon = mimicFire->GetCustomVariable<"killicon">();
			if (killIcon != nullptr) {
				//Msg("Killicon1 %s\n", killIcon);
				return killIcon;
			}
		}
		if (info.GetInflictor() != nullptr) {
			auto killIcon = info.GetInflictor()->GetCustomVariable<"killicon">();
			if (killIcon != nullptr) {
				//Msg("Killicon2 %s\n", killIcon);
				return killIcon;
			}
		}
		auto result = DETOUR_MEMBER_CALL(CTFGameRules_GetKillingWeaponName)(info, pVictim, iWeaponID);
		//Msg("Killicon %s\n", result);
		return result;
	}

	DETOUR_DECL_MEMBER(int, CTFBaseProjectile_GetDamageType)
	{
		variant_t variant;
		variant.SetInt(-1);
		reinterpret_cast<CTFBaseProjectile *>(this)->GetCustomVariableVariant<"dmgtype">(variant);
		variant.Convert(FIELD_INTEGER);
		int dmgtype = variant.Int();
		if (dmgtype != -1) {
			return dmgtype;
		}
		return DETOUR_MEMBER_CALL(CTFBaseProjectile_GetDamageType)();
	}

	DETOUR_DECL_MEMBER(void, CTFPlayer_Event_Killed, const CTakeDamageInfo& info)
	{
		auto player = reinterpret_cast<CTFPlayer *>(this);
		DETOUR_MEMBER_CALL(CTFPlayer_Event_Killed)(info);
		CBaseEntity *observer = player->m_hObserverTarget;
		if (info.GetInflictor() != nullptr) {
			if (rtti_cast<CTFPointWeaponMimic *>(info.GetInflictor()) != nullptr) {
				player->m_hObserverTarget = info.GetInflictor();
			}
		}
	}

    class CMod : public IMod
	{
	public:
		CMod() : IMod("Etc:Weapon_Mimic_Teamnum")
		{
			bulletDamageHook.DoLoad();
			MOD_ADD_DETOUR_MEMBER(CTFPointWeaponMimic_Fire, "CTFPointWeaponMimic::Fire");
			MOD_ADD_DETOUR_MEMBER(CTFPointWeaponMimic_InputFireOnce, "CTFPointWeaponMimic::InputFireOnce");
			MOD_ADD_DETOUR_MEMBER(CTFPointWeaponMimic_InputFireMultiple, "CTFPointWeaponMimic::InputFireMultiple");
			MOD_ADD_DETOUR_STATIC(CTFProjectile_Rocket_Create,  "CTFProjectile_Rocket::Create");
			MOD_ADD_DETOUR_STATIC(CTFProjectile_Arrow_Create,  "CTFProjectile_Arrow::Create");
			MOD_ADD_DETOUR_STATIC(CBaseEntity_CreateNoSpawn,  "CBaseEntity::CreateNoSpawn");
			MOD_ADD_DETOUR_MEMBER(CTFPointWeaponMimic_dtor0,  "CTFPointWeaponMimic::~CTFPointWeaponMimic [D0]");
			MOD_ADD_DETOUR_MEMBER(CTFPointWeaponMimic_dtor2,  "CTFPointWeaponMimic::~CTFPointWeaponMimic [D2]");
			MOD_ADD_DETOUR_MEMBER(CTFPointWeaponMimic_Spawn,  "CTFPointWeaponMimic::Spawn");
			MOD_ADD_DETOUR_MEMBER(CTFGameRules_GetKillingWeaponName,  "CTFGameRules::GetKillingWeaponName");
			MOD_ADD_DETOUR_MEMBER(CTFBaseProjectile_GetDamageType,  "CTFBaseProjectile::GetDamageType");
			
			MOD_ADD_DETOUR_MEMBER(CTFPlayer_Event_Killed,  "CTFPlayer::Event_Killed");
			
		}

		virtual std::vector<std::string> GetRequiredMods() { return {"Common:Weapon_Shoot"};}
	};
	CMod s_Mod;

    ConVar cvar_enable("sig_etc_weapon_mimic_teamnum", "0", FCVAR_NOTIFY,
		"Mod: weapon mimic teamnum fix",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			s_Mod.Toggle(static_cast<ConVar *>(pConVar)->GetBool());
		});
}