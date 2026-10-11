#include "audio.h"
#include "dedicated/dedicated.h"
#include "logging/logging.h"
#include "tier1/convar.h"

#include "rapidjson/error/en.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>

DECLARE_MODULE(AudioHooks)

static thread_local const char* pszAudioEventName;
static void* g_pClientAudioQueueWaitReturnAddress;
static ConVar* Cvar_mileslog_enable;
static ConVar* Cvar_ns_print_played_sounds;

using MilesStopAll_t = void (*)();
static MilesStopAll_t MilesStopAll;
using SetThreadDescription_t = HRESULT(WINAPI*)(HANDLE hThread, PCWSTR lpThreadDescription);
static SetThreadDescription_t pSetThreadDescription =
    reinterpret_cast<SetThreadDescription_t>(GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetThreadDescription"));

CModAudioManager g_ModAudioManager;

CModAudioOverride::CModAudioOverride(const std::string& data, const fs::path& path, const std::vector<std::string>& registeredEvents)
{
    rapidjson_document dataJson;
    dataJson.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(data);
    if (dataJson.HasParseError())
    {
        spdlog::error("Failed reading audio override file {}: parse error \"{}\" at offset {}", path, GetParseError_En(dataJson.GetParseError()),
                      dataJson.GetErrorOffset());
        return;
    }
    if (!dataJson.IsObject() || !dataJson.HasMember("EventId"))
    {
        spdlog::error("Failed reading audio override file {}: expected a JSON object with EventId", path);
        return;
    }

    const auto& eventIds = dataJson["EventId"];
    if (eventIds.IsString())
        EventIds.emplace_back(eventIds.GetString());
    else if (eventIds.IsArray())
    {
        for (const auto& eventId : eventIds.GetArray())
        {
            if (!eventId.IsString())
            {
                spdlog::error("Failed reading audio override file {}: EventId array must contain only strings", path);
                return;
            }
            EventIds.emplace_back(eventId.GetString());
        }
    }
    else
    {
        spdlog::error("Failed reading audio override file {}: EventId must be a string or an array of strings", path);
        return;
    }

    if (dataJson.HasMember("EventIdRegex"))
    {
        const auto& patterns = dataJson["EventIdRegex"];
        try
        {
            if (patterns.IsString())
                EventIdsRegex.emplace_back(patterns.GetString(), std::regex(patterns.GetString()));
            else if (patterns.IsArray())
            {
                for (const auto& pattern : patterns.GetArray())
                {
                    if (!pattern.IsString())
                    {
                        spdlog::error("Failed reading audio override file {}: EventIdRegex array must contain only strings", path);
                        return;
                    }
                    EventIdsRegex.emplace_back(pattern.GetString(), std::regex(pattern.GetString()));
                }
            }
            else
            {
                spdlog::error("Failed reading audio override file {}: EventIdRegex must be a string or an array of strings", path);
                return;
            }
        }
        catch (const std::regex_error& error)
        {
            spdlog::error("Malformed regex in audio override file {}: {}", path, error.what());
            return;
        }
    }

    if (dataJson.HasMember("AudioSelectionStrategy"))
    {
        const auto& strategy = dataJson["AudioSelectionStrategy"];
        if (!strategy.IsString())
        {
            spdlog::error("Failed reading audio override file {}: AudioSelectionStrategy must be a string", path);
            return;
        }
        const std::string_view name = strategy.GetString();
        if (name == "random")
            Strategy = AudioSelectionStrategy::RANDOM;
        else if (name != "sequential")
        {
            spdlog::error("Failed reading audio override file {}: AudioSelectionStrategy must be \"sequential\" or \"random\"", path);
            return;
        }
    }

    fs::path samplesFolder = path;
    samplesFolder.replace_extension();
    if (!fs::is_directory(samplesFolder))
    {
        spdlog::error("Failed reading audio override file {}: expected a sample directory with the same name without the JSON extension", path);
        return;
    }

    for (const fs::directory_entry& file : fs::recursive_directory_iterator(samplesFolder))
    {
        if (!file.is_regular_file() || _wcsicmp(file.path().extension().c_str(), L".wav") != 0)
            continue;

        // Preserve the original first-loaded override priority for nested event folders.
        const std::u8string eventIdUtf8 = file.path().parent_path().filename().u8string();
        const std::string_view eventId(reinterpret_cast<const char*>(eventIdUtf8.data()), eventIdUtf8.size());
        if (std::find(registeredEvents.begin(), registeredEvents.end(), eventId) != registeredEvents.end())
        {
            spdlog::warn("Audio sample {} targets an already overridden event, skipping", file.path());
            continue;
        }

        std::ifstream wavStream(file.path(), std::ios::binary | std::ios::ate);
        const std::streamoff fileSize = wavStream.tellg();
        if (!wavStream || fileSize < 12 || fileSize > std::numeric_limits<unsigned int>::max())
        {
            spdlog::error("Could not read WAV sample {}: invalid file size", file.path());
            continue;
        }

        // Publish only complete buffers; Miles keeps these bytes until sounds are stopped on unload.
        auto bytes = std::make_unique_for_overwrite<uint8_t[]>(static_cast<size_t>(fileSize));
        wavStream.seekg(0, std::ios::beg);
        if (!wavStream.read(reinterpret_cast<char*>(bytes.get()), fileSize) || std::memcmp(bytes.get(), "RIFF", 4) != 0 ||
            std::memcmp(bytes.get() + 8, "WAVE", 4) != 0)
        {
            spdlog::error("Could not read WAV sample {}: incomplete or invalid WAV file", file.path());
            continue;
        }
        Samples.emplace_back(static_cast<unsigned int>(fileSize), std::move(bytes));
    }

    if (Samples.empty())
    {
        spdlog::warn("Audio override {} has no valid WAV samples; keeping original sounds", path);
        return;
    }

    spdlog::info("Loaded audio override file {}", path);
    LoadedSuccessfully = true;
}

bool CModAudioManager::TryLoadDefinition(const fs::path& path, const std::string& modName)
{
    if (IsDedicatedServer())
        return true;

    std::ifstream jsonStream(path);
    if (!jsonStream)
    {
        spdlog::warn("Unable to read audio override from file {}", path);
        return false;
    }
    std::stringstream jsonBuffer;
    jsonBuffer << jsonStream.rdbuf();

    std::vector<std::string> registeredEvents;
    {
        std::lock_guard lock(m_registryMutex);
        registeredEvents.reserve(m_eventDefinitions.size());
        for (const auto& [eventId, definition] : m_eventDefinitions)
            registeredEvents.push_back(eventId);
    }
    auto definition = std::make_shared<CModAudioOverride>(jsonBuffer.str(), path, registeredEvents);
    if (!definition->LoadedSuccessfully)
        return false;

    std::lock_guard lock(m_registryMutex);
    for (const std::string& eventId : definition->EventIds)
    {
        if (!m_eventDefinitions.try_emplace(eventId, definition).second)
        {
            spdlog::warn("\"{}\" mod tried to override sound event \"{}\" but it is already overridden, skipping", modName, eventId);
            continue;
        }
        spdlog::info("Registering sound event {}", eventId);
    }
    for (const auto& [pattern, regex] : definition->EventIdsRegex)
    {
        if (!m_regexEventDefinitions.try_emplace(pattern, RegexOverride_t{regex, definition}).second)
        {
            spdlog::warn("\"{}\" mod tried to override sound event regex \"{}\" but it is already overridden, skipping", modName, pattern);
            continue;
        }
        spdlog::info("Registering sound event regex {}", pattern);
    }
    return true;
}

std::shared_ptr<CModAudioOverride> CModAudioManager::FindReplacementDefinition(const char* eventId)
{
    if (!eventId)
        return {};

    std::lock_guard lock(m_registryMutex);
    auto exact = m_eventDefinitions.find(eventId);
    if (exact != m_eventDefinitions.end())
        return exact->second;
    auto wildcard = m_eventDefinitions.find("*");
    if (wildcard != m_eventDefinitions.end())
        return wildcard->second;
    for (const auto& [pattern, override] : m_regexEventDefinitions)
    {
        if (std::regex_search(eventId, override.Pattern))
        {
            m_eventDefinitions.try_emplace(eventId, override.Definition);
            return override.Definition;
        }
    }
    return {};
}

void CModAudioManager::Clear()
{
    // Unpublish first so the audio thread cannot select more overrides while Miles drains.
    decltype(m_eventDefinitions) definitions;
    decltype(m_regexEventDefinitions) regexDefinitions;
    {
        std::lock_guard lock(m_registryMutex);
        definitions.swap(m_eventDefinitions);
        regexDefinitions.swap(m_regexEventDefinitions);
    }
    if ((!definitions.empty() || !regexDefinitions.empty()) && !IsDedicatedServer() && MilesStopAll)
    {
        MilesStopAll();
        Sleep(50);
    }
}

static bool ShouldPlayAudioEvent(const char* eventName, const CModAudioOverride& definition)
{
    for (const std::string& name : definition.EventIds)
    {
        if (name.starts_with('!') && std::string_view(name).substr(1) == eventName)
            return false;
        // The old wildcard deliberately leaves ambient/emitter events alone to avoid static noise.
        if (name == "*" && (strstr(eventName, "_amb_") || strstr(eventName, "_emit_") || strstr(eventName, "amb_")))
            return false;
    }
    return true;
}

DECLARE_HOOK(LoadSampleMetadata, mileswin64.dll + 0xF110,
             [](auto& hook, void* sample, void* audioBuffer, unsigned int audioBufferLength, int audioType) -> bool
{
    if (audioType == 0 || !pszAudioEventName)
        return hook.Original(sample, audioBuffer, audioBufferLength, audioType);

    const char* eventName = pszAudioEventName;
    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        NS::log::MILES->info("Playing event {}", eventName);

    const auto definition = g_ModAudioManager.FindReplacementDefinition(eventName);
    if (!definition || !ShouldPlayAudioEvent(eventName, *definition))
        return hook.Original(sample, audioBuffer, audioBufferLength, audioType);

    {
        std::lock_guard lock(definition->SampleSelectionMutex);
        size_t index = definition->CurrentIndex;
        if (definition->Strategy == AudioSelectionStrategy::RANDOM)
        {
            static thread_local std::mt19937 generator(std::random_device{}());
            index = std::uniform_int_distribution<size_t>(0, definition->Samples.size() - 1)(generator);
        }
        else if (++definition->CurrentIndex >= definition->Samples.size())
            definition->CurrentIndex = 0;

        const auto& selected = definition->Samples[index];
        audioBuffer = selected.second.get();
        audioBufferLength = selected.first;
    }

    *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(sample) + 0xE8) = audioBuffer;
    *reinterpret_cast<unsigned int*>(reinterpret_cast<uintptr_t>(sample) + 0xF0) = audioBufferLength;
    const bool result = hook.Original(sample, audioBuffer, audioBufferLength, 64);
    if (!result)
        NS::log::MILES->error("LoadSampleMetadata failed for overridden event {}", eventName);
    return result;
})

DECLARE_HOOK(Sub_1800294C0, mileswin64.dll + 0x294C0, [](auto& hook, void* a1, void* a2)
{
    const char* previousEventName = pszAudioEventName;
    pszAudioEventName = reinterpret_cast<const char*>(reinterpret_cast<uint64_t*>(a2)[6]);
    hook.Original(a1, a2);
    pszAudioEventName = previousEventName;
})

DECLARE_HOOK(MilesQueueGetIsRetired, mileswin64.dll + 0x33690, [](auto& hook, void* queue, unsigned int submissionId) -> unsigned int
{
    unsigned int retired = hook.Original(queue, submissionId);
    if (!retired && hook.ReturnAddress() == g_pClientAudioQueueWaitReturnAddress)
    {
        // OnRenderStart polls this eight-slot fence with Sleep(10). Keep the
        // retirement barrier without adding a full 10 ms sleep to the frame.
        do
        {
            Sleep(0);
            retired = hook.Original(queue, submissionId);
        } while (!retired);
    }
    return retired;
})

DECLARE_HOOK(Sub_18003EBD0, mileswin64.dll + 0x3EBD0, [](auto& hook, DWORD dwThreadID, const char* threadName)
{
    if (threadName)
    {
        HANDLE thread = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, dwThreadID);
        if (thread)
        {
            const std::string name(threadName);
            if (pSetThreadDescription)
                pSetThreadDescription(thread, std::wstring(name.begin(), name.end()).c_str());
            CloseHandle(thread);
        }
    }
    hook.Original(dwThreadID, threadName);
})

DECLARE_HOOK(Sub_18003BC10, mileswin64.dll + 0x3BC10, [](auto& hook, void* a1, void* a2, void* a3, void* a4, void* a5, int a6) -> char*
{
    char* result = hook.Original(a1, a2, a3, a4, a5, a6);
    if (result)
    {
        HANDLE thread = reinterpret_cast<HANDLE>(reinterpret_cast<uint64_t*>(result)[55]);
        if (thread && pSetThreadDescription)
            pSetThreadDescription(thread, L"[Miles] WASAPI Service Thread");
    }
    return result;
})

DECLARE_HOOK(MilesLog, client.dll + 0x57DAD0, [](auto& hook, int level, const char* string)
{
    if (Cvar_mileslog_enable && Cvar_mileslog_enable->GetBool())
        NS::log::MILES->info("{} - {}", level, string);
})

ON_DLL_LOAD("mileswin64.dll", MilesWin64_Audio, [](CModule module) { AudioHooks.DispatchForModule("mileswin64.dll"); })

ON_DLL_LOAD_RELIESON("engine.dll", MilesLogFuncHooks, ConVar, [](CModule module)
{ Cvar_mileslog_enable = new ConVar("mileslog_enable", "0", FCVAR_NONE, "Enables/disables whether the mileslog func should be logged"); })

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", AudioHooks, ConVar, [](CModule module)
{
    g_pClientAudioQueueWaitReturnAddress = module.Offset(0x57F4CA).RCast<void*>();
    AudioHooks.DispatchForModule("client.dll");
    Cvar_ns_print_played_sounds = new ConVar("ns_print_played_sounds", "0", FCVAR_NONE, "");
    MilesStopAll = module.Offset(0x580850).RCast<MilesStopAll_t>();
})
