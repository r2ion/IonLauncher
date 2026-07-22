#include "audio.h"
#include "dedicated/dedicated.h"

#include <fstream>
#include <ranges>
#include <sstream>
#include <utility>

CModAudioManager::CModAudioManager() : m_runtime(*this)
{
}

CModAudioManager g_ModAudioManager;

CModAudioManager::ActiveThreadState& CModAudioManager::GetActiveThreadState()
{
    static thread_local ActiveThreadState state;
    return state;
}

bool CModAudioManager::TryLoadDefinition(const fs::path& defPath, const std::string& modName)
{
    std::ifstream jsonStream(defPath);
    if (!jsonStream)
    {
        spdlog::warn("Unable to read audio override from file {}", defPath.string());
        return false;
    }

    std::stringstream jsonBuffer;
    jsonBuffer << jsonStream.rdbuf();

    std::vector<std::string> registeredEvents;
    {
        std::shared_lock lock(m_registryMutex);
        auto keys = std::views::keys(m_eventDefinitions);
        registeredEvents.assign(keys.begin(), keys.end());
    }
    auto eventDefinition = std::make_shared<ModAudioEventDefinition>(jsonBuffer.str(), defPath, registeredEvents, !IsDedicatedServer());

    if (!eventDefinition->LoadedSuccessfully)
        return false; // no logging, the constructor has probably already logged

    std::unique_lock lock(m_registryMutex);
    for (const std::string& eventId : eventDefinition->EventIds)
    {
        if (m_eventDefinitions.contains(eventId))
        {
            spdlog::warn("\"{}\" mod tried to override sound event \"{}\" but it is already overriden, skipping.", modName, eventId);
            continue;
        }
        if (eventDefinition->IsCustomEvent)
            spdlog::info("Registering custom sound event {} with {} Miles layer(s)", eventId, eventDefinition->Layers.size());
        else
            spdlog::info("Registering sound event {}", eventId);
        m_eventDefinitions.insert({eventId, eventDefinition});

        if (eventDefinition->IsCustomEvent && !eventId.empty() && eventId != "*" && eventId[0] != '!')
        {
            auto clientDefinition = std::make_shared<ClientSoundEventDefinition>(eventId);
            m_clientEventDefinitions.insert({eventId, clientDefinition});

            auto serverDefinition = std::make_shared<ServerSoundAliasDefinition>(eventId, *eventDefinition);
            m_serverAliasDefinitions.insert({eventId, serverDefinition});
            spdlog::info("Registered custom server sound alias {} (hash {:016x}, radius {}, duration {}s, tags 0x{:08x})", eventId,
                         serverDefinition->EventHash, serverDefinition->NetworkRadius, serverDefinition->DurationSeconds,
                         serverDefinition->SoundTags);

            const uint64_t eventHash = ClientSoundEventDefinition::HashEventName(eventId);
            auto [hashIter, hashInserted] = m_clientEventDefinitionsByHash.insert({eventHash, clientDefinition});
            if (!hashInserted && _stricmp(hashIter->second->GetEventName().c_str(), eventId.c_str()) != 0)
            {
                spdlog::error("Custom sound events {} and {} have the same client hash; hash-based playback of {} is unavailable",
                              hashIter->second->GetEventName(), eventId, eventId);
            }
        }
    }

    for (const auto& eventIdRegexData : eventDefinition->EventIdsRegex)
    {
        if (m_regexEventDefinitions.contains(eventIdRegexData.first))
        {
            spdlog::warn("\"{}\" mod tried to override sound event regex \"{}\" but it is already overriden, skipping.", modName,
                         eventIdRegexData.first);
            continue;
        }
        spdlog::info("Registering sound event regex {}", eventIdRegexData.first);
        m_regexEventDefinitions.insert({eventIdRegexData.first, {.Pattern = eventIdRegexData.second, .Definition = eventDefinition}});
    }

    return true;
}

std::shared_ptr<ModAudioEventDefinition> CModAudioManager::FindExactDefinition(const char* eventId)
{
    if (!eventId)
        return {};

    std::shared_lock lock(m_registryMutex);
    auto iter = m_eventDefinitions.find(eventId);
    if (iter == m_eventDefinitions.end())
        return {};

    return iter->second;
}

std::shared_ptr<ModAudioEventDefinition> CModAudioManager::FindReplacementDefinition(const char* eventId)
{
    if (!eventId)
        return {};

    std::shared_ptr<ModAudioEventDefinition> match;
    {
        std::shared_lock lock(m_registryMutex);
        auto exact = m_eventDefinitions.find(eventId);
        if (exact != m_eventDefinitions.end())
            return exact->second;

        auto wildcard = m_eventDefinitions.find("*");
        if (wildcard != m_eventDefinitions.end())
            return wildcard->second;

        for (const auto& entry : m_regexEventDefinitions)
        {
            if (std::regex_search(eventId, entry.second.Pattern))
            {
                match = entry.second.Definition;
                break;
            }
        }
    }

    if (match)
    {
        std::unique_lock lock(m_registryMutex);
        m_eventDefinitions.try_emplace(eventId, match);
    }
    return match;
}

std::shared_ptr<ClientSoundEventDefinition> CModAudioManager::FindClientSoundEventDefinition(const char* eventId)
{
    if (!eventId)
        return {};

    std::shared_lock lock(m_registryMutex);
    auto iter = m_clientEventDefinitions.find(eventId);
    if (iter == m_clientEventDefinitions.end())
        return {};

    return iter->second;
}

std::shared_ptr<ClientSoundEventDefinition> CModAudioManager::FindClientSoundEventDefinition(uint64_t eventHash)
{
    std::shared_lock lock(m_registryMutex);
    auto iter = m_clientEventDefinitionsByHash.find(eventHash);
    if (iter == m_clientEventDefinitionsByHash.end())
        return {};

    return iter->second;
}

std::shared_ptr<ServerSoundAliasDefinition> CModAudioManager::FindServerSoundAliasDefinition(const char* eventId)
{
    if (!eventId)
        return {};

    std::shared_lock lock(m_registryMutex);
    auto iter = m_serverAliasDefinitions.find(eventId);
    if (iter == m_serverAliasDefinitions.end())
        return {};

    return iter->second;
}

void CModAudioManager::BeginClientSoundDispatch()
{
    ++GetActiveThreadState().ActiveClientDispatchDepth;
}

void CModAudioManager::EndClientSoundDispatch()
{
    ActiveThreadState& state = GetActiveThreadState();
    if (state.ActiveClientDispatchDepth == 0)
        return;

    if (--state.ActiveClientDispatchDepth == 0)
        state.ActiveClientSoundEvent.reset();
}

bool CModAudioManager::HasActiveClientSoundDispatch() const
{
    return GetActiveThreadState().ActiveClientDispatchDepth != 0;
}

ClientSoundEventDefinition* CModAudioManager::ActivateClientSoundEvent(const char* eventId)
{
    ActiveThreadState& state = GetActiveThreadState();
    state.ActiveClientSoundEvent = FindClientSoundEventDefinition(eventId);
    return state.ActiveClientSoundEvent.get();
}

ClientSoundEventDefinition* CModAudioManager::ActivateClientSoundEvent(uint64_t eventHash)
{
    ActiveThreadState& state = GetActiveThreadState();
    state.ActiveClientSoundEvent = FindClientSoundEventDefinition(eventHash);
    return state.ActiveClientSoundEvent.get();
}

ClientSoundEventDefinition* CModAudioManager::GetActiveClientSoundEvent()
{
    return GetActiveThreadState().ActiveClientSoundEvent.get();
}

std::shared_ptr<ClientSoundEventDefinition> CModAudioManager::TakeActiveClientSoundEvent()
{
    return std::move(GetActiveThreadState().ActiveClientSoundEvent);
}

ServerSoundAliasDefinition* CModAudioManager::ActivateServerSoundAlias(const char* eventId)
{
    ActiveThreadState& state = GetActiveThreadState();
    state.ActiveServerSoundAlias = FindServerSoundAliasDefinition(eventId);
    return state.ActiveServerSoundAlias.get();
}

void CModAudioManager::ClearActiveServerSoundAlias()
{
    GetActiveThreadState().ActiveServerSoundAlias.reset();
}

bool CModAudioManager::TryGetServerSoundTags(const char* eventId, uint32_t& soundTags)
{
    std::shared_ptr<ServerSoundAliasDefinition> definition = FindServerSoundAliasDefinition(eventId);
    if (!definition)
        return false;

    soundTags = definition->SoundTags;
    return true;
}
