#include "modsystem/modinstaller.h"
#include "modsystem/modmanager.h"
#include "modsystem/modworkshop_inventory.h"
#include "rtech/rui/workshop_thumbnail_atlas.h"
#include "rtech/rui/workshop_thumbnail_service.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqarray.h"

#include <algorithm>
#include <charconv>

#include <array>
#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

class CModMenuSquirrel final
{
public:
	static std::optional<fs::path> FindModIcon(const Mod& mod);
	static const ModWorkshopTrackedPackage* FindWorkshopPackage(const Mod& mod, const ModWorkshopInventorySnapshot* inventory);
	static void EnsureModIconCallback();

	template <ScriptContext context> static void PushMod(HSQUIRRELVM sqvm, Mod& mod, const ModWorkshopInventorySnapshot* inventory, size_t modIndex);

	static uint64_t NextIconGeneration()
	{
		return s_IconGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
	}

private:
	static fs::path FindRemotePackageRoot(const Mod& mod);
	static std::vector<std::string> CollectModAssets(const Mod& mod);
	static void AddAsset(std::vector<std::string>& assets, std::string_view type, std::string_view value);
	static void OnIconReady(uint64_t generation, size_t slot);

	inline static std::atomic<uint64_t> s_IconGeneration = 0;
	inline static constexpr std::array<std::string_view, 4> ICON_FILENAMES = {"icon.webp", "icon.png", "icon.jpg", "icon.jpeg"};
	inline static bool s_IconCallbackInitialized = false;
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

std::optional<fs::path> CModMenuSquirrel::FindModIcon(const Mod& mod)
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
				return candidate;
		}
	}
	return std::nullopt;
}

const ModWorkshopTrackedPackage* CModMenuSquirrel::FindWorkshopPackage(const Mod& mod, const ModWorkshopInventorySnapshot* inventory)
{
	if (!inventory || mod.m_Source != ModSource::ModWorkshop || !mod.m_ManagedId || mod.m_ManagedId->empty())
		return nullptr;
	uint64_t modId = 0;
	const std::string_view text = *mod.m_ManagedId;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), modId);
	if (error != std::errc() || end != text.data() + text.size())
		return nullptr;
	const auto package = std::ranges::find(inventory->packages, modId, &ModWorkshopTrackedPackage::modId);
	return package == inventory->packages.end() ? nullptr : &*package;
}

void CModMenuSquirrel::OnIconReady(uint64_t generation, size_t slot)
{
	SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
	if (squirrel && squirrel->m_pSQVM)
		squirrel->AsyncCall("NSUICodeCallback_ModIconReady", static_cast<int>(generation), static_cast<int>(slot));
}

void CModMenuSquirrel::AddAsset(std::vector<std::string>& assets, std::string_view type, std::string_view value)
{
	if (value.empty())
		return;
	std::string& asset = assets.emplace_back();
	asset.reserve(type.size() + value.size() + 2);
	asset.append(type).append(": ").append(value);
}

void CModMenuSquirrel::EnsureModIconCallback()
{
	if (s_IconCallbackInitialized)
		return;
	CWorkshopThumbnailService::Get().SetLocalIconReadyCallback(OnIconReady);
	s_IconCallbackInitialized = true;
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
void CModMenuSquirrel::PushMod(HSQUIRRELVM sqvm, Mod& mod, const ModWorkshopInventorySnapshot* inventory, size_t modIndex)
{
	g_pSquirrel[context]->pushnewstructinstance(sqvm, 17);

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

	const ModWorkshopTrackedPackage* trackedPackage = CModMenuSquirrel::FindWorkshopPackage(mod, inventory);
	const ModWorkshopUpdateState updateState = trackedPackage ? trackedPackage->updateState : ModWorkshopUpdateState::LegacyUnknown;
	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(updateState));
	g_pSquirrel[context]->sealstructslot(sqvm, 11);

	const InstalledModRemovalInfo removalInfo = CModInstallService::Get().GetInstalledModRemovalInfo(static_cast<int>(modIndex));
	g_pSquirrel[context]->pushbool(sqvm, removalInfo.canDelete);
	g_pSquirrel[context]->sealstructslot(sqvm, 12);

	g_pSquirrel[context]->pushinteger(sqvm, removalInfo.deleteModCount);
	g_pSquirrel[context]->sealstructslot(sqvm, 13);

	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(modIndex));
	g_pSquirrel[context]->sealstructslot(sqvm, 14);

	g_pSquirrel[context]->pushbool(sqvm, CModMenuSquirrel::FindModIcon(mod).has_value() || (trackedPackage && trackedPackage->remoteThumbnail));
	g_pSquirrel[context]->sealstructslot(sqvm, 15);

	g_pSquirrel[context]->newarray(sqvm);
	for (const std::string& asset : CModMenuSquirrel::CollectModAssets(mod))
	{
		g_pSquirrel[context]->pushstring(sqvm, asset.c_str());
		g_pSquirrel[context]->arrayappend(sqvm, -2);
	}
	g_pSquirrel[context]->sealstructslot(sqvm, 16);

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

ADD_SQFUNC("int", NSRequestModIconPage, "array<int> modIndices", "Loads available local package icons into the shared image atlas.",
           ScriptContext::UI)
{
	CModMenuSquirrel::EnsureModIconCallback();
	std::vector<CWorkshopThumbnailService::LocalIconRequest> icons;
	const std::shared_ptr<const ModWorkshopInventorySnapshot> inventory = CModWorkshopInventory::Get().GetSnapshot();
	const SQObject& argument = sqvm->_stackOfCurrentFunction[1];
	if (argument._Type == OT_ARRAY && argument._VAL.asArray)
	{
		SQArray* indices = argument._VAL.asArray;
		const size_t count = std::min(static_cast<size_t>(std::max(indices->_usedSlots, 0)), CWorkshopThumbnailAtlas::SLOT_COUNT);
		icons.reserve(count);
		for (size_t slot = 0; slot < count; ++slot)
		{
			CWorkshopThumbnailService::LocalIconRequest request;
			const SQObject& value = indices->_values[slot];
			if (value._Type == OT_INTEGER)
			{
				const int index = value._VAL.asInteger;
				if (index >= 0 && static_cast<size_t>(index) < g_pModManager->m_LoadedMods.size())
				{
					const Mod& mod = g_pModManager->m_LoadedMods[static_cast<size_t>(index)];
					request.id = static_cast<uint64_t>(index) + 1;
					if (const std::optional<fs::path> icon = CModMenuSquirrel::FindModIcon(mod))
						request.path = *icon;
					else if (const ModWorkshopTrackedPackage* package = CModMenuSquirrel::FindWorkshopPackage(mod, inventory.get()))
						request.thumbnail = package->remoteThumbnail;
				}
			}
			icons.push_back(std::move(request));
		}
	}

	const uint64_t generation = CModMenuSquirrel::NextIconGeneration();
	CWorkshopThumbnailService::Get().RequestLocalPage(generation, icons);
	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(generation));
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("array<ModInfo>", NSGetModsInformation, "", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	g_pSquirrel[context]->newarray(sqvm, 0);
	const std::shared_ptr<const ModWorkshopInventorySnapshot> inventory = CModWorkshopInventory::Get().GetSnapshot();

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
	const std::shared_ptr<const ModWorkshopInventorySnapshot> inventory = CModWorkshopInventory::Get().GetSnapshot();

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
