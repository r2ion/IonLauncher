#include <engine/r2engine.h>

#include <modsystem/modmanager.h>

using WeaponReparseCommandFn = void(__fastcall*)(const CCommand&);

static WeaponReparseCommandFn s_WeaponReparseServer;

DECLARE_MODULE(WeaponReparseHooks)

DECLARE_HOOK(ConCommand_weapon_reparse, client.dll + 0x3D4930, [](auto& hook, const CCommand& arg) -> void
{
    g_pModManager->InvalidateKeyValuesPatches("scripts/weapons/");
    if (s_WeaponReparseServer && g_pServerState && *g_pServerState >= server_state_t::ss_active)
        s_WeaponReparseServer(arg);
    hook.Original(arg);
});

DECLARE_HOOK(ConCommand_weapon_reparse_server, server.dll + 0x6D1B70, [](auto& hook, const CCommand& arg) -> void
{
    g_pModManager->InvalidateKeyValuesPatches("scripts/weapons/");
    hook.Original(arg);
});

ON_DLL_LOAD("server.dll", WeaponReparse_Server, [](CModule module)
{
    NOTE_UNUSED(module);
    DISPATCH_MODULE(WeaponReparseHooks)
    s_WeaponReparseServer = HookSys::GetOriginalFunction<WeaponReparseCommandFn>(HookSys::FindHook("ConCommand_weapon_reparse_server"));
});

ON_DLL_LOAD("client.dll", WeaponReparse_Client, [](CModule module) { DISPATCH_MODULE(WeaponReparseHooks) });
