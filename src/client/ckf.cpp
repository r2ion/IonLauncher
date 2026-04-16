#include "ckf.h"
#include "r2client.h"
#include "cliententitylist.h"
std::vector<int> crouchCodes;
std::vector<int> jumpCodes;

InputHolder crouchHolder;
InputHolder jumpHolder;

long long jumptime;
long long crouchtime;

KeyInfo_t* v_KeyInfoArray = nullptr;

DECLARE_MODULE(CKFHooks)
ConVar* Cvar_ckf_enabled = nullptr;
ConVar* Cvar_ckf_logging = nullptr;
void FindBinds()
{
	auto hook = HookSys::FindHook("CInputSystem__PostEvent");
	v_CInputSystem__PostEvent = HookSys::GetOriginalFunction<CInputSystem__PostEvent>(hook);
	crouchCodes.clear();
	jumpCodes.clear();

	for (int i = 0; i < BUTTON_CODE_COUNT; i++)
	{
		KeyInfo_t keyInfo = v_KeyInfoArray[i];

		char* bind = keyInfo.m_pKeyBinding;

		if (IsBadReadPtr2(bind))
			continue;

		if (strcmp(bind, "+ability 3") == 0) // jump bind
		{
			jumpCodes.push_back(i);
		}
		else if (strcmp(bind, "+ability 4") == 0) // jump bind
		{
			jumpCodes.push_back(i);
		}
		else if (strcmp(bind, "+duck") == 0)
		{
			crouchCodes.push_back(i);
		}
		else if (strcmp(bind, "+toggle_duck") == 0)
		{
			crouchCodes.push_back(i);
		}
	}
}

uint64_t lastCrouchKickTime = 0;

void CFKPostEvent(void* thisObject, InputEventType_t nType, int nTick, int data1, int data2, int data3) {
	if (!Cvar_ckf_enabled->GetBool())
		return;
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);

	long long real = (ts.tv_nsec / 1000) + (ts.tv_sec * 1000000);
	if (nType == IE_ButtonPressed)
	{
		if (std::find(jumpCodes.begin(), jumpCodes.end(), data1) != jumpCodes.end() && !jumpHolder.waitingToSend)
			{
				jumpHitTime = real;
				long sinceCrouch = real - crouchHolder.timestamp;
				if (crouchHolder.waitingToSend && sinceCrouch <= CROUCHKICK_FIX_BUFFER_MICROSECONDS)
				{
					crouchHolder.Release();
					if (Cvar_ckf_logging->GetBool())
						spdlog::info("crouchkick: {}ms CROUCH IS EARLY", sinceCrouch / 1000.0f);
					jumpSentTime = real;
				}
				else
				{
					jumpHolder.Hold(thisObject, nType, nTick, data1, data2, data3);
					jumpHolder.timestamp = real;
					
					return;
				}
			}
			else if (std::find(crouchCodes.begin(), crouchCodes.end(), data1) != crouchCodes.end() && !crouchHolder.waitingToSend)
			{
			
				crouchHitTime = real;
				long sinceJump = real - jumpHolder.timestamp;
				if (jumpHolder.waitingToSend && sinceJump < CROUCHKICK_FIX_BUFFER_MICROSECONDS)
				{
					jumpHolder.Release();
					if (Cvar_ckf_logging->GetBool())
						spdlog::info("crouchkick: {}ms JUMP IS EARLY", sinceJump / 1000.0f);
					jumpSentTime = real;
				}
				else
				{
					crouchHolder.Hold(thisObject, nType, nTick, data1, data2, data3);
					crouchHolder.timestamp = real;
					return;
				}
			}
		}
		else if (nType == IE_ButtonReleased)
		{
			if (std::find(crouchCodes.begin(), crouchCodes.end(), data1) != crouchCodes.end())
			{
				if (crouchHolder.waitingToSend)
				{
					crouchHolder.Release();
				}
			}
			if (std::find(jumpCodes.begin(), jumpCodes.end(), data1) != jumpCodes.end())
			{
				if (jumpHolder.waitingToSend)
				{
					jumpHolder.Release();
				}
			}
		}
}

DECLARE_HOOK(EngineUpdate, engine.dll + 0x77f50, [](auto& hook)
{
	hook.Original();
	if (!Cvar_ckf_enabled->GetBool())
		return;
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	long long real = (ts.tv_nsec / 1000) + (ts.tv_sec * 1000000);
	if (jumpHolder.waitingToSend)
	{
		long sinceJump = real - jumpHolder.timestamp;

		if (sinceJump > CROUCHKICK_FIX_BUFFER_MICROSECONDS)
		{
			jumpHolder.Release();
			jumpSentTime = real;

			long long e = jumpHolder.timestamp - crouchtime;
			if (e < 100000)
			{
				if (Cvar_ckf_logging->GetBool())
					spdlog::info("not crouchkick: {}ms CROUCH IS EARLY", e / 1000.0f);
			}

			jumptime = jumpHolder.timestamp;
		}
	}

	if (crouchHolder.waitingToSend)
	{
		long sinceCrouch = real - crouchHolder.timestamp;
		if (sinceCrouch > CROUCHKICK_FIX_BUFFER_MICROSECONDS)
		{
			crouchHolder.Release();

			long long e = crouchHolder.timestamp - jumptime;

			if (e < 100000)
			{
				if (Cvar_ckf_logging->GetBool())
					spdlog::info("not crouchkick: {}ms JUMP IS EARLY", e / 1000.0f);
			}

			crouchtime = crouchHolder.timestamp;
		}
	}
});

ADD_SQFUNC("void", StartWallrun, "", "", ScriptContext::CLIENT)
{
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	wallrunStartedTime = (ts.tv_nsec / 1000) + (ts.tv_sec * 1000000);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("integer", GetWallkickTiming, "", "", ScriptContext::CLIENT)
{
	g_pSquirrel[ScriptContext::CLIENT]->pushinteger(sqvm, jumpSentTime - wallrunStartedTime);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("vector", GetWallNormalVector, "entity", "", ScriptContext::CLIENT) {
	auto ent = g_pSquirrel[ScriptContext::CLIENT]->getentity<uintptr_t>(sqvm, 1);
	Vector3 wallNormalVector = *(Vector3*)(((uintptr_t)ent + 0x2BA0));

	g_pSquirrel[ScriptContext::CLIENT]->pushvector(sqvm, wallNormalVector);
	return SQRESULT_NOTNULL;

}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll",CKFEngine,ConVar,[](CModule module)
{
	v_KeyInfoArray = module.Offset(0x1396C5C0).RCast<KeyInfo_t*>();
	DISPATCH_MODULE(CKFHooks);

	Cvar_ckf_enabled = new ConVar("ckf_enabled", "1", FCVAR_ARCHIVE_PLAYERPROFILE | FCVAR_CLIENTDLL, "Enable crouch kick fix. 1 = enabled, 0 = disabled.");
	Cvar_ckf_logging = new ConVar(
			"ckf_logging",
			"0",
			FCVAR_ARCHIVE_PLAYERPROFILE | FCVAR_CLIENTDLL,
			"Enable crouch kick fix logging. 1 = enabled, 0 = disabled.");
	})


