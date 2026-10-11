#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

enum class AudioSelectionStrategy
{
    SEQUENTIAL,
    RANDOM
};

class CModAudioOverride
{
  public:
    CModAudioOverride(const std::string& data, const fs::path& path, const std::vector<std::string>& registeredEvents);

    bool LoadedSuccessfully = false;
    std::vector<std::string> EventIds;
    std::vector<std::pair<std::string, std::regex>> EventIdsRegex;
    std::vector<std::pair<unsigned int, std::unique_ptr<uint8_t[]>>> Samples;
    AudioSelectionStrategy Strategy = AudioSelectionStrategy::SEQUENTIAL;
    size_t CurrentIndex = 0;
    std::mutex SampleSelectionMutex;
};

class CModAudioManager
{
  public:
    bool TryLoadDefinition(const fs::path& path, const std::string& modName);
    void Clear();
    std::shared_ptr<CModAudioOverride> FindReplacementDefinition(const char* eventId);

  private:
    struct EventNameHash_t
    {
        using is_transparent = void;
        size_t operator()(std::string_view name) const
        {
            return std::hash<std::string_view>{}(name);
        }
    };

    struct RegexOverride_t
    {
        std::regex Pattern;
        std::shared_ptr<CModAudioOverride> Definition;
    };

    std::mutex m_registryMutex;
    std::unordered_map<std::string, std::shared_ptr<CModAudioOverride>, EventNameHash_t, std::equal_to<>> m_eventDefinitions;
    std::unordered_map<std::string, RegexOverride_t> m_regexEventDefinitions;
};

extern CModAudioManager g_ModAudioManager;
