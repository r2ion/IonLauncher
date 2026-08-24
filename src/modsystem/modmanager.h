#pragma once
#include "tier1/convar.h"
#include "tier0/memstd.h"
#include "vscript/squirrel/squirrel.h"

#include "rapidjson/document.h"
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <span>
#include "mod.h"
#include "tier0/vanilla.h"

namespace fs = std::filesystem;

class CModule;
class CModelLoader;
class KeyValues;

const fs::path MOD_FOLDER_SUFFIX = "mods";
const fs::path PACKAGE_MOD_FOLDER_SUFFIX = "packages";
const fs::path REMOTE_MOD_FOLDER_SUFFIX = "runtime\\remote\\mods";
const fs::path MOD_OVERRIDE_DIR = "mod";
const fs::path COMPILED_ASSETS_SUFFIX = "runtime\\compiled";
const fs::path MOD_ICONS_SUFFIX = "runtime\\icons";

const std::set<std::string> MODS_BLACKLIST = {"Mod Settings"};

struct ModOverrideFile
{
public:
	Mod* m_pOwningMod;
	fs::path m_Path;
};

class ModManager
{
private:
	bool m_bHasLoadedMods = false;
	bool m_bHasEnabledModsCfg = false;
	bool m_bRuntimeUnloadedForFilesystemMutation = false;
	rapidjson_document m_EnabledModsCfg;
	std::string cfgPath;
	int manifestoVersion = 0;
	std::unordered_set<std::string> m_ModModelFiles;
	std::unordered_set<std::string> m_ModLooseModelFiles;
	std::unordered_map<std::string, std::string> m_ModVpkModelSources;
	std::unordered_set<std::string> m_StaleModModelFiles;
	mutable std::mutex m_ModelReloadMutex;
	bool m_bModelReloadPending = false;
	CModelLoader* m_pModelLoader = nullptr;
	std::unordered_map<std::string, bool> m_EnabledStateOverrides;

	void LoadMods();
	bool UnloadMods(bool unloadRpaksNow);
	void RunModelReload();
	void RegisterLooseModelReloadPath(const fs::path& path);
	static std::string PackagePathKey(const fs::path& path);
	static bool IsPathAtOrBelow(const fs::path& path, const fs::path& root);
	std::string NormaliseModelLookupPath(const fs::path& path) const;
	std::vector<std::string> GetModelReloadPaths() const;
	void MarkModelsReloaded(const std::unordered_set<std::string>& failedPaths);

	// precalculated hashes
	size_t m_hScriptsRsonHash;
	size_t m_hPdefHash;
	size_t m_hKBActHash;

public:
	std::vector<Mod> m_LoadedMods;
	std::unordered_map<std::string, ModOverrideFile> m_ModFiles;
	std::unordered_map<std::string, VanillaCompatibility::CompatibilityMode> m_CompiledAssetFiles;
	std::unordered_set<std::string> m_CompiledFiles;
	std::unordered_map<std::string, std::string> m_DependencyConstants;
	std::unordered_set<std::string> m_PluginDependencyConstants;

private:
	/**
	 * Discovers all mods from disk, and loads their initial state.
	 *
	 * This searches for mods in various ways and loads the mods configuration from
	 * disk, populating `m_LoadedMods`. Note that this does not clear `m_LoadedMods`
	 * before doing work.
	 *
	 * @returns nothing
	 **/
	void DiscoverMods();

	/**
	 * Saves mod enabled state to enabledmods.json file.
	 *
	 * This loops over loaded mods (stored in `m_LoadedMods` list), exports their
	 * state (enabled or disabled) to a local JSON document, then exports this
	 * document to local profile.
	 *
	 * @returns nothing
	 **/
	void ExportModsConfigurationToFile();

	/**
	 * Load information for all mods from filesystem.
	 *
	 * This looks for mods in several directories (expecting them to be formatted in
	 * some way); it then uses respective `mod.json` manifest files to create `Mod`
	 * instances, which are then stored in the `m_LoadedMods` variable.
	 *
	 * @returns nothing
	 **/
	void SearchFilesystemForMods();

	/**
	 * Prevents crashes caused by mods being installed several times.
	 *
	 * Whether through manual install or remote mod downloading, several versions of
	 * a same mod can be located in the current profile: enabling all of them would
	 * lead to a crash, due to some files loaded several times.
	 *
	 * This checks the local `m_LoadedMods` mods list for multiple versions of a
	 * same mod: if so, this disables all versions of the relevant mod.
	 *
	 * @returns nothing
	 **/
	void DisableMultipleModVersions();

	/**
	 * Builds the modinfo object for sending to the masterserver.
	 *
	 * @returns nothing
	 **/
	void BuildModInfo();
	bool IsSafeKeyValuesDumpPath(const fs::path& path);
	void AppendWeaponModNames(KeyValues& keyValues, std::vector<std::string>& weaponModNames);
	void MergeKeyValuesRoots(KeyValues& keyValues, const KeyValues& baseKeyValues);
	bool WriteKeyValuesTextFile(const fs::path& path, const std::string& contents);
	bool WriteWeaponModOrderFile(const fs::path& path, const char* rootName, const std::vector<std::string>& weaponModOrder);
	bool ReadConditionalKeyValues(const fs::path& filePath, bool keepNorthstar, std::string& contents);
	static bool EvaluateGameModeKeyValuesSymbol(const char* symbol);

public:
	explicit ModManager(const CModule& engineModule);
	void ReloadMods();
	bool UnloadModsForFilesystemMutation();
	void RequestModelReload();
	std::unordered_map<std::string, bool> CaptureEnabledStatesForPackages(std::span<const fs::path> packageRoots) const;
	void ReloadModsWithEnabledStates(std::unordered_map<std::string, bool> enabledStates);
	bool HasLoadedPackageMods(const fs::path& packageRoot, std::span<const std::string> expectedModNames) const;
	void RegisterMountedVPKModels(const ModVPKEntry& vpkEntry);
	bool IsModModelFile(const fs::path& path) const;
	bool GetModVPKModelSource(const fs::path& path, std::string& vpkPath) const;
	std::string NormaliseModFilePath(const fs::path path) const;
	void CompileAssetsForFile(const char* filename);

	void DeleteRemoteMod(const char* modName, const char* version);

	// compile asset type stuff, these are done in files under runtime/compiled/
	void BuildScriptsRson();
	void BuildLocalPackageIcons();
	void DumpCompiledKeyValues();
	void TryBuildKeyValues(const char* filename);
	void BuildPdef();
	void BuildKBActionsList();
};

fs::path GetModFolderPath();
fs::path GetRemoteModFolderPath();
fs::path GetPackageFolderPath();
fs::path GetCompiledAssetsPath();
fs::path GetModIconPath();

extern ModManager* g_pModManager;
