#include "core/filesystem/filesystem.h"
#include "modsystem/modmanager.h"
#include "tier0/vanilla.h"
#include "tier1/keyvalues.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

static bool IsSafeKeyValuesDumpPath(const fs::path& path)
{
    if (path.empty() || path.is_absolute())
        return false;

    return std::ranges::none_of(path, [](const fs::path& component) { return component == ".."; });
}

static void AppendWeaponModNames(KeyValues& keyValues, std::vector<std::string>& weaponModNames)
{
    for (KeyValues* root = &keyValues; root; root = root->m_pPeer)
    {
        KeyValues* mods = root->FindKey("Mods");
        if (!mods)
            continue;

        for (const KeyValues* weaponMod = mods->m_pSub; weaponMod; weaponMod = weaponMod->m_pPeer)
        {
            const char* weaponModName = weaponMod->GetName();
            if (!weaponModName || std::ranges::find(weaponModNames, weaponModName) != weaponModNames.end())
                continue;

            weaponModNames.emplace_back(weaponModName);
        }
    }
}
static void MergeKeyValuesRoots(KeyValues& keyValues, const KeyValues& baseKeyValues)
{
    for (const KeyValues* baseRoot = &baseKeyValues; baseRoot; baseRoot = baseRoot->m_pPeer)
        keyValues.RecursiveMergeKeyValues(*baseRoot);
}

static bool WriteKeyValuesTextFile(const fs::path& path, const std::string& contents)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;

    output << contents;
    return output.good();
}

static bool WriteWeaponModOrderFile(const fs::path& path, const char* rootName, const std::vector<std::string>& weaponModOrder)
{
    KeyValues orderKeyValues(rootName);
    KeyValues* mods = orderKeyValues.FindKey("Mods", true);
    for (const std::string& weaponModName : weaponModOrder)
        mods->AddSubKey(new KeyValues(weaponModName.c_str()));

    return orderKeyValues.SaveToFile(path.string().c_str());
}

static bool ReadConditionalKeyValues(const fs::path& filePath, const bool keepNorthstar, std::string& contents)
{
    std::ifstream input(filePath, std::ios::binary);
    if (!input)
        return false;

    contents.clear();
    std::string line;
    bool inConditional = false;
    bool keepBlock = true;

    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.find("///if VANILLA") != std::string::npos)
        {
            inConditional = true;
            keepBlock = !keepNorthstar;
            continue;
        }
        if (line.find("///if NORTHSTAR") != std::string::npos)
        {
            inConditional = true;
            keepBlock = keepNorthstar;
            continue;
        }
        else if (line.find("///endif") != std::string::npos)
        {
            inConditional = false;
            keepBlock = true;
            continue;
        }

        if (!inConditional || keepBlock)
        {
            contents.append(line);
            contents.push_back('\n');
        }
    }

    return !input.bad();
}

void ModManager::DumpCompiledKeyValues()
{
    std::vector<std::string> keyValuesPaths;
    for (const Mod& mod : m_LoadedMods)
    {
        if (!mod.m_bEnabled)
            continue;

        for (const auto& [fileHash, path] : mod.KeyValues)
        {
            NOTE_UNUSED(fileHash);
            keyValuesPaths.push_back(path);
        }
    }

    std::ranges::sort(keyValuesPaths);
    keyValuesPaths.erase(std::unique(keyValuesPaths.begin(), keyValuesPaths.end()), keyValuesPaths.end());

    for (const std::string& path : keyValuesPaths)
        CompileAssetsForFile(path.c_str());

    const fs::path dumpDirectory = GetCompiledAssetsPath().parent_path() / "compiled_keyvalues_dump";
    std::error_code error;
    fs::remove_all(dumpDirectory, error);
    if (error)
    {
        spdlog::error("Failed to clear compiled KeyValues dump directory {}: {}", dumpDirectory.string(), error.message());
        return;
    }

    fs::create_directories(dumpDirectory, error);
    if (error)
    {
        spdlog::error("Failed to create compiled KeyValues dump directory {}: {}", dumpDirectory.string(), error.message());
        return;
    }

    size_t dumpedFiles = 0;
    size_t failedFiles = 0;
    for (const auto& [normalisedPath, compatibilityMode] : m_CompiledAssetFiles)
    {
        NOTE_UNUSED(compatibilityMode);

        const fs::path relativePath = normalisedPath;
        if (!IsSafeKeyValuesDumpPath(relativePath))
        {
            spdlog::warn("Refusing to dump compiled KeyValues with unsafe path {}.", relativePath.string());
            ++failedFiles;
            continue;
        }

        const fs::path compiledPath = GetCompiledAssetsPath() / relativePath;
        std::ifstream input(compiledPath, std::ios::binary);
        if (!input)
        {
            spdlog::warn("Could not read compiled KeyValues file {}.", compiledPath.string());
            ++failedFiles;
            continue;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        const std::string compiledContents = buffer.str();

        const std::string resourceName = relativePath.generic_string();
        KeyValues resolvedKeyValues(resourceName.c_str());
        resolvedKeyValues.UsesEscapeSequences(true);
        if (!KeyValues_LoadFromBuffer(&resolvedKeyValues, resourceName.c_str(), compiledContents.c_str(), g_pFilesystem))
        {
            spdlog::warn("Could not parse compiled KeyValues file {}.", resourceName);
            ++failedFiles;
            continue;
        }

        const fs::path outputPath = dumpDirectory / relativePath;
        fs::create_directories(outputPath.parent_path(), error);
        if (error)
        {
            spdlog::warn("Could not create compiled KeyValues dump directory {}: {}", outputPath.parent_path().string(), error.message());
            error.clear();
            ++failedFiles;
            continue;
        }

        if (!resolvedKeyValues.SaveToFile(outputPath.string().c_str()))
        {
            spdlog::warn("Could not write resolved KeyValues file {}.", outputPath.string());
            ++failedFiles;
            continue;
        }

        ++dumpedFiles;
    }

    spdlog::info("Dumped {} resolved compiled KeyValues files to {} ({} failed).", dumpedFiles, dumpDirectory.string(), failedFiles);
}

void ModManager::TryBuildKeyValues(const char* filename)
{
    spdlog::info("Building KeyValues for file {}", filename);

    const std::string normalisedPath = NormaliseModFilePath(fs::path(filename));
    const fs::path keyValuesPath(normalisedPath);
    const fs::path compiledPath = GetCompiledAssetsPath() / keyValuesPath;
    const fs::path compiledDirectory = compiledPath.parent_path();
    fs::create_directories(compiledDirectory);

    const std::string originalContents = ReadVPKFile(filename, FileSourceType_ModOverride | FileSourceType_Original);
    if (originalContents.empty())
    {
        spdlog::warn("Tried to patch KeyValues file {} but no base file was found.", filename);
        return;
    }

    KeyValues originalKeyValues(normalisedPath.c_str());
    originalKeyValues.UsesEscapeSequences(true);
    if (!KeyValues_LoadFromBuffer(&originalKeyValues, normalisedPath.c_str(), originalContents.c_str(), g_pFilesystem))
    {
        spdlog::warn("Could not parse original KeyValues file {}.", filename);
        return;
    }

    const char* parsedRootName = originalKeyValues.GetName();
    if (!parsedRootName || !*parsedRootName)
    {
        spdlog::warn("Could not determine the root name of KeyValues file {}.", filename);
        return;
    }

    const std::string rootName(parsedRootName);
    const bool isWeaponData = rootName == "WeaponData";
    const bool keepNorthstar = !g_pVanillaCompatibility->GetVanillaCompatibility();
    const size_t fileHash = STR_HASH(normalisedPath);
    KeyValues compiledPatchKeyValues(rootName.c_str());
    compiledPatchKeyValues.UsesEscapeSequences(true);
    std::vector<std::string> requiredWeaponModOrder;
    std::vector<std::string> optionalWeaponModOrder;
    std::vector<std::string> compiledDependencies;
    bool foundPatch = false;

    for (int64_t index = static_cast<int64_t>(m_LoadedMods.size()) - 1; index >= 0; --index)
    {
        Mod& mod = m_LoadedMods[index];
        if (!mod.m_bEnabled)
            continue;

        const auto modKeyValues = mod.KeyValues.find(fileHash);
        if (modKeyValues == mod.KeyValues.end())
            continue;

        const fs::path patchPath = mod.m_ModDirectory / "keyvalues" / fs::path(modKeyValues->second);
        std::string patchContents;
        if (!ReadConditionalKeyValues(patchPath, keepNorthstar, patchContents))
        {
            spdlog::warn("Could not read KeyValues patch {} from mod {}.", patchPath.string(), mod.Name);
            return;
        }

        KeyValues patchKeyValues(normalisedPath.c_str());
        patchKeyValues.UsesEscapeSequences(true);
        if (!KeyValues_LoadFromBuffer(&patchKeyValues, normalisedPath.c_str(), patchContents.c_str(), g_pFilesystem))
        {
            spdlog::warn("Could not parse KeyValues patch {} from mod {}.", patchPath.string(), mod.Name);
            return;
        }

        if (isWeaponData)
        {
            AppendWeaponModNames(patchKeyValues, mod.RequiredOnClient ? requiredWeaponModOrder : optionalWeaponModOrder);
        }

        MergeKeyValuesRoots(compiledPatchKeyValues, patchKeyValues);
        foundPatch = true;
    }

    if (!foundPatch)
        return;

    const std::string patchFileName = "mod_patch_" + keyValuesPath.filename().string();
    if (!compiledPatchKeyValues.SaveToFile((compiledDirectory / patchFileName).string().c_str()))
    {
        spdlog::warn("Could not write compiled KeyValues patch {}.", patchFileName);
        return;
    }
    compiledDependencies.push_back(NormaliseModFilePath(keyValuesPath.parent_path() / patchFileName));
    const std::string patchBase = "#base \"" + patchFileName + "\"\n";

    const std::string originalFileName = "mod_original_" + keyValuesPath.filename().string();
    if (!WriteKeyValuesTextFile(compiledDirectory / originalFileName, originalContents))
    {
        spdlog::warn("Could not write original KeyValues file {}.", originalFileName);
        return;
    }
    compiledDependencies.push_back(NormaliseModFilePath(keyValuesPath.parent_path() / originalFileName));

    std::string compiledContents = "// AUTOGENERATED: MOD PATCH KV\n";
    if (isWeaponData)
    {
        std::vector<std::string> weaponModOrder = std::move(requiredWeaponModOrder);
        AppendWeaponModNames(originalKeyValues, weaponModOrder);
        for (const std::string& weaponModName : optionalWeaponModOrder)
        {
            if (std::ranges::find(weaponModOrder, weaponModName) == weaponModOrder.end())
                weaponModOrder.push_back(weaponModName);
        }

        if (!weaponModOrder.empty())
        {
            const std::string orderFileName = "mod_order_" + keyValuesPath.filename().string();
            if (!WriteWeaponModOrderFile(compiledDirectory / orderFileName, rootName.c_str(), weaponModOrder))
            {
                spdlog::warn("Could not write weapon mod order file {}.", orderFileName);
                return;
            }

            compiledContents += "#base \"" + orderFileName + "\"\n";
            compiledDependencies.push_back(NormaliseModFilePath(keyValuesPath.parent_path() / orderFileName));
        }
    }

    compiledContents += patchBase;
    compiledContents += "#base \"" + originalFileName + "\"\n";
    compiledContents += "\"" + rootName + "\"\n{\n}\n";
    if (!WriteKeyValuesTextFile(compiledPath, compiledContents))
    {
        spdlog::warn("Could not write compiled KeyValues file {}.", compiledPath.string());
        return;
    }

    const VanillaCompatibility::CompatibilityMode compatibilityMode = g_pVanillaCompatibility->GetVanillaCompatibility()
                                                                          ? VanillaCompatibility::CompatibilityMode::Vanilla
                                                                          : VanillaCompatibility::CompatibilityMode::Northstar;
    m_CompiledAssetFiles.insert_or_assign(normalisedPath, compatibilityMode);
    m_CompiledFiles.insert(normalisedPath);
    m_CompiledFiles.insert(compiledDependencies.begin(), compiledDependencies.end());
}
