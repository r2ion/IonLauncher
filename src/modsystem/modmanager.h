#pragma once
#include "tier1/convar.h"
#include "tier0/memstd.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include "tier1/keyvalues.h"

#include "rapidjson/document.h"
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <span>
#include "mod.h"

namespace fs = std::filesystem;

namespace ModPaths
{
bool Equal(const fs::path& left, const fs::path& right);
bool IsAtOrBelow(const fs::path& path, const fs::path& root);
} // namespace ModPaths

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
	std::unordered_set<std::string> m_RegisteredVPKs;
	std::unordered_map<std::string, std::string> m_MapVpkFileSources;
	std::unordered_set<std::string> m_MapVpkCacheBypassFiles;
	std::unordered_set<std::string> m_StaleMapVpkModelFiles;
	mutable std::mutex m_ModelReloadMutex;
	bool m_bModelReloadPending = false;
	CModelLoader* m_pModelLoader = nullptr;
	std::unordered_map<std::string, bool> m_EnabledStateOverrides;

	struct KeyValuesPatch_t
	{
		fs::path m_Path;
		std::string m_Contents;
	};
	struct KeyValuesPatchSet_t
	{
		std::vector<KeyValuesPatch_t> m_Patches;
		bool m_bLoaded = false;
	};
	std::mutex m_KeyValuesMutex;
	std::unordered_map<std::string, std::shared_ptr<KeyValuesPatchSet_t>> m_KeyValuesPatches;
	void BuildKeyValuesPatchIndex();
	std::shared_ptr<const KeyValuesPatchSet_t> GetKeyValuesPatches(const char* resourceName);

	void LoadMods();
	bool UnloadMods(bool unloadRpaksNow);
	void RunModelReload();
    void RegisterLooseModelReloadPath(const fs::path& path);
    std::string NormaliseModelLookupPath(const fs::path& path) const;
	std::vector<std::string> GetModelReloadPaths() const;
	void MarkModelsReloaded(const std::unordered_set<std::string>& failedPaths);
	std::unordered_set<std::string> FlushModelPaths(std::span<const std::string> paths);

	// precalculated hashes
	size_t m_hScriptsRsonHash;
	size_t m_hPdefHash;
	size_t m_hKBActHash;

public:
	std::vector<Mod> m_LoadedMods;
	std::unordered_map<std::string, ModOverrideFile> m_ModFiles;
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

public:
	explicit ModManager(const CModule& engineModule);
	void ReloadMods();
	bool UnloadModsForFilesystemMutation(std::span<const fs::path> packageRoots);
	void RequestModelReload();
	std::unordered_map<std::string, bool> CaptureEnabledStatesForPackages(std::span<const fs::path> packageRoots) const;
	void ReloadModsWithEnabledStates(std::unordered_map<std::string, bool> enabledStates);
	bool HasLoadedPackageMods(const fs::path& packageRoot, std::span<const std::string> expectedModNames) const;
	void RegisterMountedVPKModels(const ModVPKEntry& vpkEntry);
	void UnregisterMountedVPKModels(const char* vpkPath);
	bool IsMapVPKCacheFile(const fs::path& path) const;
	bool GetMapVPKFileSource(const fs::path& path, std::string& vpkPath) const;
	// Only called after old-world unreference/material cleanup, before new map assets bind.
	bool PrepareMapVPKs(const char* mapName);
	bool MountMapVPKs(const char* mapName);
	bool NeedsMapVPKTransition(const char* mapName, bool forceReload) const;
	bool IsModModelFile(const fs::path& path) const;
	bool GetModVPKModelSource(const fs::path& path, std::string& vpkPath) const;
	std::string NormaliseModFilePath(const fs::path path) const;
	void CompileAssetsForFile(const char* filename);
	void DeleteRemoteMod(const char* modName, const char* version);
	void BuildScriptsRson();
	void BuildLocalPackageIcons();
	void DumpCompiledKeyValues();
	bool ApplyKeyValuesPatches(KeyValues& keyValues, const char* resourceName, KeyValuesLoadFromTextBuffer_t loadFromBuffer,
		IBaseFileSystem* fileSystem, const char* pathID, KeyValuesEvaluateSymbol_t evaluateSymbol, int flags);
	void InvalidateKeyValuesPatches(const char* pathPrefix);
	void BuildPdef();
	void BuildKBActionsList();
};

fs::path GetModFolderPath();
fs::path GetRemoteModFolderPath();
fs::path GetPackageFolderPath();
fs::path GetCompiledAssetsPath();
fs::path GetGeneratedAssetsPath();
fs::path GetModIconPath();

extern ModManager* g_pModManager;
