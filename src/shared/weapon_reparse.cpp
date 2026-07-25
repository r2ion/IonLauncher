#include <modsystem/modmanager.h>
#include <string>
#include <algorithm>

DECLARE_MODULE(WeaponReparseHooks)

static void RemoveCompiledWeaponScripts()
{
	// avoid clearing literally everything, just weapons
	auto& files = g_pModManager->m_CompiledFiles;
	std::erase_if(
		files, [](const std::string& val) { return val.starts_with("scripts/weapons/") || val.starts_with("scripts\\weapons\\"); });
}

DECLARE_HOOK(ConCommand_weapon_reparse, client.dll + 0x19B2C0, [](auto& hook, const CCommand& arg) -> void
{
	RemoveCompiledWeaponScripts();
	hook.Original(arg);
});


DECLARE_HOOK(ConCommand_weapon_reparse_server, server.dll + 0x6D2B70, [](auto& hook, const CCommand& arg) -> void
{
	RemoveCompiledWeaponScripts();
	hook.Original(arg);
});


ON_DLL_LOAD("server.dll", WeaponReparse_Server, [](CModule module)
{
	DISPATCH_MODULE(WeaponReparseHooks)
});

ON_DLL_LOAD("client.dll", WeaponReparse_Client, [](CModule module)
{
    DISPATCH_MODULE(WeaponReparseHooks)
});
