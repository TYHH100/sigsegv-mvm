#include "mod.h"

namespace Mod::MvM::Blu_Velocity_Limit_Remove
{
    
	constexpr uint8_t s_Buf_PostThink[] = {
		// +0000 mov     [esp], ebx
		// +0001 call    CBaseEntity::GetTeamNumber(void)
		// +0006
		// +0009 cmp     eax, 3
		0x53,
		0xE8, 0x00, 0x00, 0x00, 0x00,
		0x83, 0xC4, 0x10,
		0x83, 0xF8, 0x03
	};
	
	struct CPatch_CTFPlayer_PostThink : public CPatch
	{
		CPatch_CTFPlayer_PostThink() : CPatch(sizeof(s_Buf_PostThink)) {}
		
		virtual const char *GetFuncName() const override { return "CTFPlayer::PostThink"; }
		virtual uint32_t GetFuncOffMin() const override  { return 0x0100; }
		virtual uint32_t GetFuncOffMax() const override  { return 0x0500; }
		
		virtual bool GetVerifyInfo(ByteBuf& buf, ByteBuf& mask) const override
		{
			buf.CopyFrom(s_Buf_PostThink);
			
			buf.SetDword(0x01 +1, (uint32_t)AddrManager::GetAddr("CBaseEntity::GetTeamNumber"));
			
			/* allow any 3-bit source register code */
			mask[0x00 + 1] = 0b11000111;

			mask[0x06 + 1] = 0x00;
			
			mask.SetDword(0x01 + 1, 0x00000000);
			
			return true;
		}
		
		virtual bool GetPatchInfo(ByteBuf& buf, ByteBuf& mask) const override
		{
			/* replace 'TF_TEAM_BLUE'*/
			buf[0x09 + 0] = 0x83;
			buf[0x09 + 1] = 0xF8;
			buf[0x09 + 2] = 0xFF;
			
			mask.SetRange(0x09, 3, 0xff);
			
			return true;
		}
	};

    class CMod : public IMod
	{
	public:
		CMod() : IMod("MvM:Blu_Velocity_Limit_Remove")
		{
			this->AddPatch(new CPatch_CTFPlayer_PostThink());
        }
    };
	CMod s_Mod;

    ConVar cvar_enable("sig_mvm_blu_velocity_limit_remove", "0", FCVAR_NOTIFY | FCVAR_GAMEDLL,
		"Mod: remove velocity limit for blu team",
		[](IConVar *pConVar, const char *pOldValue, float flOldValue){
			s_Mod.Toggle(static_cast<ConVar *>(pConVar)->GetBool());
		});
}