#include "eiface.h"
#include "engine/server/player_limits.h"
#include "engine/server/server_limits.h"
#include "game/server/player.h"
#include "game/server/util_server.h"
#include "game/shared/usercmd.h"
#include "game/shared/weapon_prediction.h"
#include "vstdlib/random.h"

static IVEngineServer** g_pRunUserCmdsEngineServer;
static ConVar** g_pSvUsercmdMaxQueued;

DECLARE_MODULE(PlayerCommandHooks)

// clang-format off
DECLARE_HOOK(CPlayerMove::RunCommand, server.dll + 0x5B8100,
([](auto& hook, void* self, CPlayer* player, SV_CUserCmd* pUserCmd, uint64_t a4)
// clang-format on
{
	if (g_pServerLimits->Cvar_sv_antispeedhack_enable->GetBool())
	{
		CClient* pClient = &g_pClientArray[player->m_nPlayerIndex - 1];

		if (g_pServerLimits->m_PlayerLimitData.find(pClient) != g_pServerLimits->m_PlayerLimitData.end())
		{
			PlayerLimitData* pLimitData = &g_pServerLimits->m_PlayerLimitData[pClient];

			pLimitData->flFrameUserCmdBudget = fmax(0.0, pLimitData->flFrameUserCmdBudget - pUserCmd->frameTime);

			if (pLimitData->flFrameUserCmdBudget <= 0.0)
			{
				spdlog::warn("player {} went over usercmd budget ({})", pClient->m_szClientName, pLimitData->flFrameUserCmdBudget);
				return;
			}
		}
	}

    CPlayerExtended* const prediction = player ? player->GetPlayerExtended() : nullptr;
    if (!prediction)
    {
        hook.Original(self, player, pUserCmd, a4);
        return;
    }
    WeaponPredictionCommand_t command;
    InitializeWeaponPredictionCommand(command, player, pUserCmd);
    RunWeaponPredictionCommand(command, [&]
    {
        hook.Original(self, player, pUserCmd, a4);
        SendWeaponPredictionSnapshot(command);
    });
}))

DECLARE_HOOK(CPlayer::RunQueuedUserCommands, server.dll + 0x5A6E50, [](auto& hook, CPlayer* player, int maximum, bool firstIteration) -> bool
{
    if (firstIteration && maximum > 0 && player->m_CommandContext.Count() > 0 && (*g_pRunUserCmdsEngineServer)->ShouldMoveOneCommandPerClientFrame())
    {
        const int queuedLimit = (*g_pSvUsercmdMaxQueued)->GetInt();
        if (queuedLimit >= 0)
        {
            int pending = 0;
            for (const CCommandContext& context : player->m_CommandContext)
            {
                if (context.cmds.Count() == 0)
                    continue;

                for (int i = context.numcmds - 1; i >= 0; --i)
                {
                    if (static_cast<int>(context.cmds[i].command_number) > player->m_latestCommandRun)
                        ++pending;
                }
            }
            if (pending - maximum > queuedLimit)
                maximum = pending - queuedLimit;
        }
    }
    return hook.Original(player, maximum, firstIteration);
})

ON_DLL_LOAD("server.dll", PlayerCommandLimits, [](CModule module)
{
    g_pRunUserCmdsEngineServer = module.Offset(0xBFBD98).RCast<IVEngineServer**>();
    g_pSvUsercmdMaxQueued = module.Offset(0x12106C8).RCast<ConVar**>();
    DISPATCH_HOOK(PlayerCommandHooks, CPlayerMove::RunCommand)
    DISPATCH_HOOK(PlayerCommandHooks, CPlayer::RunQueuedUserCommands)
})

static HMODULE g_RunUserCmdsServerModule = 0;
static decltype(&::RandomIntZeroMax) g_pRunUserCmdsRandomIntZeroMax = nullptr;

// lazy rebuild
// clang-format off
DECLARE_HOOK(RunUserCmds, server.dll + 0x483D10,
[](auto& hook, bool a1, float a2)
// clang-format on
{
	NOTE_UNUSED(hook);
	unsigned char v3; // bl
	int v5; // er14
	int i; // edi
	__int64 v7; // rax
	DWORD* v8; // rbx
	int v9; // edi
	__int64* v10; // rsi
	__int64 v11; // rax
	int v12; // er12
	__int64 v13; // rdi
	int v14; // ebx
	int v15; // eax
	__int64 v16; // r8
	int v17; // edx
	char v18; // r15
	char v19; // bp
	int v20; // esi
	__int64* v21; // rdi
	__int64 v22; // rcx
	bool v23; // al
	__int64 v24; // rax
	__int64 v25[NEW_MAX_PLAYERS]; // [rsp+20h] [rbp-138h] BYREF

	uintptr_t base = (__int64)g_RunUserCmdsServerModule;
	auto g_pGlobals = *(__int64*)(base + 0xBFBE08);
	__int64 globals = g_pGlobals;

	IVEngineServer* g_pEngineServer = *reinterpret_cast<IVEngineServer**>(base + 0xBFBD98);

	auto qword_1814D9648 = *(__int64*)(base + 0x14D9648);
	auto qword_1814DA408 = *(__int64*)(base + 0x14DA408);
	auto qword_1812107E8 = *(__int64*)(base + 0x12107E8);
	auto qword_1812105A8 = *(__int64*)(base + 0x12105A8);

	auto sub_180485590 = (void(__fastcall*)(__int64))(base + 0x485590);
	auto sub_18058CD80 = (void(__fastcall*)(__int64))(base + 0x58CD80);
	auto sub_1805A6D90 = (void(__fastcall*)(__int64))(base + 0x5A6D90);
	auto sub_1805A6E50 = (bool(__fastcall*)(__int64, int, char))(base + 0x5A6E50);
	auto sub_1805A6C20 = (void(__fastcall*)(__int64))(base + 0x5A6C20);

	v3 = *(unsigned char*)(g_pGlobals + 73);
    if (*(DWORD*)(qword_1814D9648 + 92) && (g_pEngineServer->IsDedicatedServer() || !*(DWORD*)(qword_1814DA408 + 92)) && v3)
	{
		globals = g_pGlobals;
		v5 = 1;
		for (i = 1; i <= *(DWORD*)(g_pGlobals + 52); ++i)
		{
			v7 = reinterpret_cast<__int64>(UTIL_PlayerByIndex(i));
			v8 = (DWORD*)v7;
			if (v7)
			{
				*(__int64*)(base + 0x1210420) = v7;
				*(float*)(g_pGlobals + 16) = a2;
				if (!a1)
					sub_18058CD80(v7);
				sub_1805A6D90((__int64)v8);
			}
			globals = g_pGlobals;
		}
		memset(v25, 0, sizeof(v25));
		v9 = 0;
		if (*(int*)(globals + 52) > 0)
		{
			v10 = v25;
			do
			{
				v11 = reinterpret_cast<__int64>(UTIL_PlayerByIndex(++v9));
				globals = g_pGlobals;
				*v10++ = v11;
			} while (v9 < *(DWORD*)(globals + 52));
		}
		v12 = *(DWORD*)(qword_1812107E8 + 92);
		if (*(DWORD*)(qword_1812105A8 + 92))
		{
			v13 = *(DWORD*)(globals + 52) - 1;
			if (v13 >= 1)
			{
				v14 = *(DWORD*)(globals + 52);
				do
				{
					v15 = g_pRunUserCmdsRandomIntZeroMax();
					v16 = v25[v13--];
					v17 = v15 % v14--;
					v25[v13 + 1] = v25[v17];
					v25[v17] = v16;
				} while (v13 >= 1);
				globals = g_pGlobals;
			}
		}
		v18 = 1;
		do
		{
			v19 = 0;
			v20 = 0;
			if (*(int*)(globals + 52) > 0)
			{
				v21 = v25;
				do
				{
					v22 = *v21;
					if (*v21)
					{
						*(__int64*)(base + 0x1210420) = *v21;
						*(float*)(globals + 16) = a2;
						v23 = sub_1805A6E50(v22, v12, v18);
						globals = g_pGlobals;
						if (v23)
							v19 = 1;
						else
							*v21 = 0;
					}
					++v20;
					++v21;
				} while (v20 < *(DWORD*)(globals + 52));
			}
			v18 = 0;
		} while (v19);
		if (*(int*)(globals + 52) >= 1)
		{
			do
			{
				v24 = reinterpret_cast<__int64>(UTIL_PlayerByIndex(v5));
				if (v24)
				{
					*(__int64*)(base + 0x1210420) = v24;
					*(float*)(g_pGlobals + 16) = a2;
					sub_1805A6C20(v24);
				}
				++v5;
			} while (v5 <= *(DWORD*)(g_pGlobals + 52));
		}
		sub_180485590(*(__int64*)(base + 0xB7B2D8));
	}
})

ON_DLL_LOAD_RELIESON("server.dll", MaxPlayersUserCommands, ServerUtilities, [](CModule module)
{
    if (!MaxPlayersIncreaseEnabled())
        return;

    g_RunUserCmdsServerModule = (HMODULE)module.GetModuleBase();
    g_pRunUserCmdsRandomIntZeroMax =
        reinterpret_cast<decltype(g_pRunUserCmdsRandomIntZeroMax)>(GetProcAddress(GetModuleHandleA("vstdlib.dll"), "RandomIntZeroMax"));
    DISPATCH_HOOK(PlayerCommandHooks, RunUserCmds)
})
