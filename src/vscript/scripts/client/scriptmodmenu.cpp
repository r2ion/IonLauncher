#include "modsystem/modinstaller.h"
#include "modsystem/modmanager.h"
#include "modsystem/modinventory.h"
#include "modsystem/platform/modworkshop.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <algorithm>

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

class CModMenuSquirrel final
{
public:
	static bool FindModIcon(const Mod& mod, fs::path& iconPath);
	static const ModTrackedPackage* FindManagedPackage(const Mod& mod, const ModInventorySnapshot* inventory);

	template <ScriptContext context> static void PushMod(HSQUIRRELVM sqvm, Mod& mod, const ModInventorySnapshot* inventory, size_t modIndex);


private:
	static fs::path FindRemotePackageRoot(const Mod& mod);
	static std::vector<std::string> CollectModAssets(const Mod& mod);
	static void AddAsset(std::vector<std::string>& assets, std::string_view type, std::string_view value);
	inline static constexpr std::array<std::string_view, 4> ICON_FILENAMES = {"icon.webp", "icon.png", "icon.jpg", "icon.jpeg"};
};

fs::path CModMenuSquirrel::FindRemotePackageRoot(const Mod& mod)
{
	if (!mod.IsRemote())
		return {};
	const fs::path remoteRoot = GetRemoteModFolderPath().lexically_normal();
	const fs::path relative = mod.m_ModDirectory.lexically_normal().lexically_relative(remoteRoot);
	const auto first = relative.begin();
	if (relative.empty() || relative == "." || first == relative.end() || *first == "..")
		return {};
	return remoteRoot / *first;
}

bool CModMenuSquirrel::FindModIcon(const Mod& mod, fs::path& iconPath)
{
	const std::array<fs::path, 3> roots = {mod.m_PackageDirectory, mod.m_ModDirectory, FindRemotePackageRoot(mod)};
	for (const fs::path& root : roots)
	{
		if (root.empty())
			continue;
		for (const std::string_view filename : ICON_FILENAMES)
		{
			const fs::path candidate = root / filename;
			std::error_code error;
			if (fs::is_regular_file(candidate, error) && !error)
			{
				iconPath = fs::canonical(candidate, error);
				if (!error)
					return true;
			}
		}
	}
	return false;
}

const ModTrackedPackage* CModMenuSquirrel::FindManagedPackage(const Mod& mod, const ModInventorySnapshot* inventory)
{
	if (!inventory || !mod.m_ManagedId || mod.m_ManagedId->empty())
		return nullptr;
	const auto package = std::ranges::find_if(inventory->packages, [&mod](const ModTrackedPackage& candidate) {
		return candidate.source == mod.m_Source && candidate.packageId == *mod.m_ManagedId;
	});
	return package == inventory->packages.end() ? nullptr : &*package;
}


void CModMenuSquirrel::AddAsset(std::vector<std::string>& assets, std::string_view type, std::string_view value)
{
	if (value.empty())
		return;
	std::string& asset = assets.emplace_back();
	asset.reserve(type.size() + value.size() + 2);
	asset.append(type).append(": ").append(value);
}


std::vector<std::string> CModMenuSquirrel::CollectModAssets(const Mod& mod)
{
	std::vector<std::string> assets;

	for (const ModScript& script : mod.Scripts)
		AddAsset(assets, "Script", script.Path);
	AddAsset(assets, "Init script", mod.initScript);
	for (const ModVPKEntry& vpk : mod.Vpks)
		AddAsset(assets, "VPK", vpk.m_sVpkPath);
	for (const ModRpakEntry& rpak : mod.Rpaks)
		AddAsset(assets, "RPAK", rpak.m_pakName);
	for (const std::string& path : mod.LocalisationFiles)
		AddAsset(assets, "Localization", path);
	for (const std::string& path : mod.BinkVideos)
		AddAsset(assets, "Bink", path);
	for (const auto& [hash, path] : mod.KeyValues)
	{
		NOTE_UNUSED(hash);
		AddAsset(assets, "KeyValues", path);
	}
	AddAsset(assets, "PDiff", mod.Pdiff);

	std::ranges::sort(assets);
	assets.erase(std::ranges::unique(assets).begin(), assets.end());
	return assets;
}

template <ScriptContext context>
void CModMenuSquirrel::PushMod(HSQUIRRELVM sqvm, Mod& mod, const ModInventorySnapshot* inventory, size_t modIndex)
{
	g_pSquirrel[context]->pushnewstructinstance(sqvm, 21);

	// name
	g_pSquirrel[context]->pushstring(sqvm, mod.Name.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 0);

	// description
	g_pSquirrel[context]->pushstring(sqvm, mod.Description.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 1);

	// version
	g_pSquirrel[context]->pushstring(sqvm, mod.Version.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 2);

	// download link
	g_pSquirrel[context]->pushstring(sqvm, mod.DownloadLink.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 3);

	// load priority
	g_pSquirrel[context]->pushinteger(sqvm, mod.LoadPriority);
	g_pSquirrel[context]->sealstructslot(sqvm, 4);

	// enabled
	g_pSquirrel[context]->pushbool(sqvm, mod.m_bEnabled);
	g_pSquirrel[context]->sealstructslot(sqvm, 5);

	// required on client
	g_pSquirrel[context]->pushbool(sqvm, mod.RequiredOnClient);
	g_pSquirrel[context]->sealstructslot(sqvm, 6);

	// is remote
	g_pSquirrel[context]->pushbool(sqvm, mod.IsRemote());
	g_pSquirrel[context]->sealstructslot(sqvm, 7);

	// convars
	g_pSquirrel[context]->newarray(sqvm);
	for (ModConVar* cvar : mod.ConVars)
	{
		g_pSquirrel[context]->pushstring(sqvm, cvar->Name.c_str());
		g_pSquirrel[context]->arrayappend(sqvm, -2);
	}
	g_pSquirrel[context]->sealstructslot(sqvm, 8);

	const std::string managedId = mod.m_ManagedId.value_or("");
	g_pSquirrel[context]->pushstring(sqvm, managedId.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 9);

	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(mod.m_Source));
	g_pSquirrel[context]->sealstructslot(sqvm, 10);

	const ModTrackedPackage* trackedPackage = CModMenuSquirrel::FindManagedPackage(mod, inventory);
	const ModUpdateState updateState = trackedPackage ? trackedPackage->updateState : ModUpdateState::LegacyUnknown;
	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(updateState));
	g_pSquirrel[context]->sealstructslot(sqvm, 11);

	const InstalledModRemovalInfo removalInfo = CModInstallService::Get().GetInstalledModRemovalInfo(static_cast<int>(modIndex));
	g_pSquirrel[context]->pushbool(sqvm, removalInfo.canDelete);
	g_pSquirrel[context]->sealstructslot(sqvm, 12);

	g_pSquirrel[context]->pushinteger(sqvm, removalInfo.deleteModCount);
	g_pSquirrel[context]->sealstructslot(sqvm, 13);

	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(modIndex));
	g_pSquirrel[context]->sealstructslot(sqvm, 14);

	fs::path iconPath;
	const bool hasLocalIcon = CModMenuSquirrel::FindModIcon(mod, iconPath);
	g_pSquirrel[context]->pushbool(sqvm, hasLocalIcon || (trackedPackage && (!trackedPackage->remoteThumbnailUrl.empty() || trackedPackage->remoteThumbnail)));
	g_pSquirrel[context]->sealstructslot(sqvm, 15);

	g_pSquirrel[context]->newarray(sqvm);
	for (const std::string& asset : CModMenuSquirrel::CollectModAssets(mod))
	{
		g_pSquirrel[context]->pushstring(sqvm, asset.c_str());
		g_pSquirrel[context]->arrayappend(sqvm, -2);
	}
	g_pSquirrel[context]->sealstructslot(sqvm, 16);

	const std::u8string iconPathUtf8 = hasLocalIcon ? iconPath.u8string() : std::u8string();
	g_pSquirrel[context]->pushstring(sqvm, reinterpret_cast<const char*>(iconPathUtf8.c_str()));
	g_pSquirrel[context]->sealstructslot(sqvm, 17);
	std::string iconUrl = trackedPackage ? trackedPackage->remoteThumbnailUrl : std::string();
	std::string iconVersion = trackedPackage ? trackedPackage->remoteUpdatedAt : std::string();
	std::string iconFallbackUrl;
	if (trackedPackage && trackedPackage->remoteThumbnail)
	{
		const ModWorkshopThumbnail& thumbnail = *trackedPackage->remoteThumbnail;
		if (iconUrl.empty())
			iconUrl = CModWorkshopClient::BuildThumbnailUrl(thumbnail);
		iconVersion = thumbnail.updatedAt;
		if (thumbnail.hasThumbnail)
		{
			ModWorkshopThumbnail original = thumbnail;
			original.hasThumbnail = false;
			iconFallbackUrl = CModWorkshopClient::BuildThumbnailUrl(original);
		}
	}
	g_pSquirrel[context]->pushstring(sqvm, iconUrl.c_str());
	g_pSquirrel[context]->sealstructslot(sqvm, 18);
	g_pSquirrel[context]->pushstring(sqvm, iconVersion.c_str());
	g_pSquirrel[context]->sealstructslot(sqvm, 19);
	g_pSquirrel[context]->pushstring(sqvm, iconFallbackUrl.c_str());
	g_pSquirrel[context]->sealstructslot(sqvm, 20);

	// add current object to squirrel array
	g_pSquirrel[context]->arrayappend(sqvm, -2);
}

ADD_SQFUNC("void", NSDeleteRemoteMod, "string modName, string modVersion", "", ScriptContext::CLIENT | ScriptContext::UI)
{
	const SQChar* modName = g_pSquirrel[context]->getstring(sqvm, 1);
	const SQChar* modVersion = g_pSquirrel[context]->getstring(sqvm, 2);
	g_pModManager->DeleteRemoteMod(modName, modVersion);

	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSRemoveMod, "int modIndex", "Queues safe removal of the indexed installed mod.", ScriptContext::UI)
{
	const int modIndex = static_cast<int>(g_pSquirrel[context]->getinteger(sqvm, 1));
	const bool queued = CModInstallService::Get().RequestInstalledModRemoval(modIndex);
	g_pSquirrel[context]->pushbool(sqvm, queued);
	return SQRESULT_NOTNULL;
}


ADD_SQFUNC("array<ModInfo>", NSGetModsInformation, "", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	g_pSquirrel[context]->newarray(sqvm, 0);
	const std::shared_ptr<const ModInventorySnapshot> inventory = CModInventory::Get().GetSnapshot();

	for (size_t modIndex = 0; modIndex < g_pModManager->m_LoadedMods.size(); ++modIndex)
	{
		CModMenuSquirrel::PushMod<context>(sqvm, g_pModManager->m_LoadedMods[modIndex], inventory.get(), modIndex);
	}

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("array<ModInfo>", NSGetModInformation, "string modName", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	const SQChar* modName = g_pSquirrel[context]->getstring(sqvm, 1);
	g_pSquirrel[context]->newarray(sqvm, 0);
	const std::shared_ptr<const ModInventorySnapshot> inventory = CModInventory::Get().GetSnapshot();

	for (size_t modIndex = 0; modIndex < g_pModManager->m_LoadedMods.size(); ++modIndex)
	{
		Mod& mod = g_pModManager->m_LoadedMods[modIndex];
		if (mod.Name.compare(modName) != 0)
			continue;
		CModMenuSquirrel::PushMod<context>(sqvm, mod, inventory.get(), modIndex);
	}

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("array<string>", NSGetModNames, "", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	g_pSquirrel[context]->newarray(sqvm, 0);

	for (Mod& mod : g_pModManager->m_LoadedMods)
	{
		g_pSquirrel[context]->pushstring(sqvm, mod.Name.c_str());
		g_pSquirrel[context]->arrayappend(sqvm, -2);
	}

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC(
	"void",
	NSSetModEnabled,
	"string modName, string modVersion, bool enabled",
	"",
	ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	const SQChar* modName = g_pSquirrel[context]->getstring(sqvm, 1);
	const SQChar* modVersion = g_pSquirrel[context]->getstring(sqvm, 2);
	const SQBool enabled = g_pSquirrel[context]->getbool(sqvm, 3);

	// manual lookup, not super performant but eh not a big deal
	for (Mod& mod : g_pModManager->m_LoadedMods)
	{
		if (!mod.Name.compare(modName) && !mod.Version.compare(modVersion))
		{
			mod.m_bEnabled = enabled;
			return SQRESULT_NULL;
		}
	}

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSReloadMods, "", "", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	g_pModManager->ReloadMods();
	return SQRESULT_NULL;
}
