#include "core/filesystem/filesystem.h"
#include "modsystem/modmanager.h"
#include "tier1/keyvalues.h"

#include <algorithm>
#include <fstream>
#include <utility>

uint32_t KeyValuesNameSymbol(const KeyValues& key)
{
    return key.m_iKeyNameCaseSensitive1 | (uint32_t(key.m_iKeyNameCaseSensitive2) << 8);
}

class CKeyValuesPatchMerge
{
    struct Children_t
    {
        std::unordered_map<uint32_t, KeyValues*> m_ByName;
        KeyValues** m_Tail;
    };
    std::unordered_map<KeyValues*, Children_t> m_Children;

  public:
    void Merge(KeyValues& destination, KeyValues& source)
    {
        if (!source.m_pSub)
            return;

        auto [entry, inserted] = m_Children.try_emplace(&destination);
        Children_t& children = entry->second;
        if (inserted)
        {
            children.m_Tail = &destination.m_pSub;
            for (KeyValues* child = destination.m_pSub; child; child = child->m_pPeer)
            {
                children.m_ByName.try_emplace(KeyValuesNameSymbol(*child), child);
                children.m_Tail = &child->m_pPeer;
            }
        }

        KeyValues** link = &source.m_pSub;
        while (KeyValues* child = *link)
        {
            const auto [match, added] = children.m_ByName.try_emplace(KeyValuesNameSymbol(*child), child);
            if (added)
            {
                *link = child->m_pPeer;
                child->m_pPeer = nullptr;
                *children.m_Tail = child;
                children.m_Tail = &child->m_pPeer;
            }
            else
            {
                Merge(*match->second, *child);
                link = &child->m_pPeer;
            }
        }
    }
};

void AppendWeaponModNames(KeyValues& keyValues, std::vector<uint32_t>& names, std::unordered_set<uint32_t>& seen)
{
    for (KeyValues* root = &keyValues; root; root = root->m_pPeer)
    {
        if (KeyValues* mods = root->FindKey("Mods"))
        {
            for (KeyValues* mod = mods->m_pSub; mod; mod = mod->m_pPeer)
            {
                const uint32_t name = KeyValuesNameSymbol(*mod);
                if (seen.insert(name).second)
                    names.push_back(name);
            }
        }
    }
}

void ModManager::BuildKeyValuesPatchIndex()
{
    std::scoped_lock lock(m_KeyValuesMutex);
    m_KeyValuesPatches.clear();
    for (auto mod = m_LoadedMods.rbegin(); mod != m_LoadedMods.rend(); ++mod)
    {
        if (!mod->m_bEnabled)
            continue;
        for (const auto& [hash, path] : mod->KeyValues)
        {
            NOTE_UNUSED(hash);
            auto& patches = m_KeyValuesPatches[NormaliseModFilePath(path)];
            if (!patches)
                patches = std::make_shared<KeyValuesPatchSet_t>();
            patches->m_Patches.push_back({mod->m_ModDirectory / "keyvalues" / fs::path(path), {}, mod->RequiredOnClient});
        }
    }
}

void ModManager::InvalidateKeyValuesPatches(const char* pathPrefix)
{
    const std::string prefix = NormaliseModFilePath(pathPrefix);
    std::scoped_lock lock(m_KeyValuesMutex);
    for (auto& [path, patches] : m_KeyValuesPatches)
    {
        if (!path.starts_with(prefix) || !patches->m_bLoaded)
            continue;

        if (patches.use_count() != 1)
        {
            auto replacement = std::make_shared<KeyValuesPatchSet_t>();
            replacement->m_Patches.reserve(patches->m_Patches.size());
            for (const KeyValuesPatch_t& patch : patches->m_Patches)
                replacement->m_Patches.push_back({patch.m_Path, {}, patch.m_bRequiredOnClient});
            patches = std::move(replacement);
        }
        patches->m_bLoaded = false;
    }
}

std::shared_ptr<const ModManager::KeyValuesPatchSet_t> ModManager::GetKeyValuesPatches(const char* resourceName)
{
    const std::string path = NormaliseModFilePath(resourceName);
    std::scoped_lock lock(m_KeyValuesMutex);
    const auto entry = m_KeyValuesPatches.find(path);
    if (entry == m_KeyValuesPatches.end())
        return {};

    const auto& patches = entry->second;
    if (!patches->m_bLoaded)
    {
        for (KeyValuesPatch_t& patch : patches->m_Patches)
        {
            std::ifstream input(patch.m_Path, std::ios::binary | std::ios::ate);
            const std::streamoff size = input ? input.tellg() : std::streampos(-1);
            if (size < 0)
            {
                spdlog::warn("Could not read KeyValues patch {}.", patch.m_Path);
                return {};
            }
            patch.m_Contents.resize(static_cast<size_t>(size));
            input.seekg(0);
            if (!input.read(patch.m_Contents.data(), size))
            {
                spdlog::warn("Could not read KeyValues patch {}.", patch.m_Path);
                return {};
            }
        }
        patches->m_bLoaded = true;
    }

	return patches;
}

bool ModManager::ApplyKeyValuesPatches(KeyValues& keyValues, const char* resourceName, KeyValuesLoadFromTextBuffer_t loadFromBuffer,
                                       IBaseFileSystem* fileSystem, const char* pathID, KeyValuesEvaluateSymbol_t evaluateSymbol, int flags)
{
    const auto patches = GetKeyValuesPatches(resourceName);
    if (!patches)
        return true;

    const bool isWeaponData = !strcmp(keyValues.GetName(), "WeaponData");
    KeyValues merged(keyValues.GetName());
    merged.UsesEscapeSequences(keyValues.m_bHasEscapeSequences != 0);
    CKeyValuesPatchMerge merger;
    std::vector<uint32_t> requiredNames, originalNames, optionalNames;
    std::unordered_set<uint32_t> requiredSeen, originalSeen, optionalSeen;
    if (isWeaponData)
        AppendWeaponModNames(keyValues, originalNames, originalSeen);

    for (const KeyValuesPatch_t& patch : patches->m_Patches)
    {
        KeyValues parsed(keyValues.GetName());
        parsed.UsesEscapeSequences(keyValues.m_bHasEscapeSequences != 0);
        if (!loadFromBuffer(&parsed, resourceName, patch.m_Contents.c_str(), fileSystem, pathID, evaluateSymbol, flags))
        {
            spdlog::warn("Could not parse KeyValues patch {}.", patch.m_Path);
            return false;
        }
        if (isWeaponData)
            AppendWeaponModNames(parsed, patch.m_bRequiredOnClient ? requiredNames : optionalNames,
                                 patch.m_bRequiredOnClient ? requiredSeen : optionalSeen);
        for (KeyValues* root = &parsed; root; root = root->m_pPeer)
            merger.Merge(merged, *root);
    }

    merger.Merge(merged, keyValues);
    delete keyValues.m_pSub;
    keyValues.m_pSub = std::exchange(merged.m_pSub, nullptr);

    if (isWeaponData)
    {
        if (KeyValues* mods = keyValues.FindKey("Mods"))
        {
            std::unordered_map<uint32_t, std::pair<KeyValues*, KeyValues*>> byName;
            for (KeyValues* mod = mods->m_pSub; mod;)
            {
                KeyValues* next = mod->m_pPeer;
                auto& [head, tail] = byName[KeyValuesNameSymbol(*mod)];
                if (tail)
                    tail->m_pPeer = mod;
                else
                    head = mod;
                tail = mod;
                mod->m_pPeer = nullptr;
                mod = next;
            }
            KeyValues** tail = &mods->m_pSub;
            for (const auto* names : {&originalNames, &requiredNames, &optionalNames})
            {
                for (const uint32_t name : *names)
                {
                    const auto found = byName.find(name);
                    if (found == byName.end())
                        continue;
                    *tail = found->second.first;
                    tail = &found->second.second->m_pPeer;
                    byName.erase(found);
                }
            }
            *tail = nullptr;
        }
    }
    return true;
}

bool ModManager::IsSafeKeyValuesDumpPath(const fs::path& path)
{
    return !path.empty() && !path.is_absolute() && std::ranges::none_of(path, [](const fs::path& component) { return component == ".."; });
}

void ModManager::DumpCompiledKeyValues()
{
    std::vector<std::string> paths;
    {
        std::scoped_lock lock(m_KeyValuesMutex);
        paths.reserve(m_KeyValuesPatches.size());
        for (const auto& [path, patches] : m_KeyValuesPatches)
            paths.push_back(path);
    }
    std::ranges::sort(paths);

    const fs::path dumpDirectory = GetCompiledAssetsPath().parent_path() / "compiled_keyvalues_dump";
    std::error_code error;
    fs::remove_all(dumpDirectory, error);
    if (error)
    {
        spdlog::error("Failed to clear KeyValues dump directory {}: {}", dumpDirectory, error.message());
        return;
    }

    size_t dumped = 0, failed = 0;
    for (const std::string& path : paths)
    {
        if (!IsSafeKeyValuesDumpPath(path))
        {
            spdlog::warn("Refusing to dump KeyValues with unsafe path {}.", path);
            ++failed;
            continue;
        }
        const std::string contents = ReadVPKFile(path.c_str());
        KeyValues resolved(path.c_str());
        resolved.UsesEscapeSequences(true);
        if (contents.empty() || !KeyValues_LoadFromBuffer(&resolved, path.c_str(), contents.c_str(), g_pFilesystem))
        {
            spdlog::warn("Could not load KeyValues file {}.", path);
            ++failed;
            continue;
        }
        const fs::path outputPath = dumpDirectory / fs::path(path);
        fs::create_directories(outputPath.parent_path(), error);
        if (error || !resolved.SaveToFile(outputPath.string().c_str()))
        {
            spdlog::warn("Could not write resolved KeyValues file {}.", outputPath);
            error.clear();
            ++failed;
            continue;
        }
        ++dumped;
    }
    spdlog::info("Dumped {} resolved KeyValues files to {} ({} failed).", dumped, dumpDirectory, failed);
}
