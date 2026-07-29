#pragma once

#include "audio_runtime.h"

#include <shared_mutex>

class CModAudioManager
{
  public:
    class CActiveClientSoundDispatch
    {
      public:
        explicit CActiveClientSoundDispatch(CModAudioManager& manager) : m_manager(manager)
        {
            m_manager.BeginClientSoundDispatch();
        }
        ~CActiveClientSoundDispatch()
        {
            m_manager.EndClientSoundDispatch();
        }

        CActiveClientSoundDispatch(const CActiveClientSoundDispatch&) = delete;
        CActiveClientSoundDispatch& operator=(const CActiveClientSoundDispatch&) = delete;

      private:
        CModAudioManager& m_manager;
    };

    using PlayResult = AudioPlayResult;

    CModAudioManager();
    ~CModAudioManager() = default;
    CModAudioManager(const CModAudioManager&) = delete;
    CModAudioManager& operator=(const CModAudioManager&) = delete;

    bool TryLoadDefinition(const fs::path&, const std::string& modName);
    void Clear();
    std::shared_ptr<ModAudioEventDefinition> FindReplacementDefinition(const char* eventId);

    ClientSoundEventDefinition* ActivateClientSoundEvent(const char* eventId);
    ClientSoundEventDefinition* ActivateClientSoundEvent(uint64_t eventHash);
    ClientSoundEventDefinition* GetActiveClientSoundEvent();
    std::shared_ptr<ClientSoundEventDefinition> TakeActiveClientSoundEvent();
    bool HasActiveClientSoundDispatch() const;

    ServerSoundAliasDefinition* ActivateServerSoundAlias(const char* eventId);
    void ClearActiveServerSoundAlias();
    bool HasServerSoundAlias(const char* eventId)
    {
        return FindServerSoundAliasDefinition(eventId) != nullptr;
    }
    bool TryGetServerSoundTags(const char* eventId, uint32_t& soundTags);
    bool TryGetReplacementSample(const char* eventName, void*& data, unsigned int& dataLength, int& decoderType)
    {
        return m_runtime.TryGetReplacementSample(eventName, data, dataLength, decoderType);
    }
    bool IsCustomEventControl(const char* eventName) const
    {
        return m_runtime.IsControlName(eventName);
    }

    PlayResult TryPlayCustomEvent(void* eventSystem, const char* eventName, uint64_t eventId, const void* eventContext)
    {
        return m_runtime.TryPlayEvent(eventSystem, eventName, eventId, eventContext);
    }
    void ApplyCustomEventControl(const char* controlName, const void* eventContext)
    {
        m_runtime.ApplyEventControl(controlName, eventContext);
    }
    void UpdateCustomEventPosition(uint64_t eventId, float x, float y, float z)
    {
        m_runtime.UpdateEventPosition(eventId, x, y, z);
    }
    void UpdateCustomEventOrientation(uint64_t eventId, float facingX, float facingY, float facingZ, float upX, float upY, float upZ)
    {
        m_runtime.UpdateEventOrientation(eventId, facingX, facingY, facingZ, upX, upY, upZ);
    }
    void UpdateCustomEventController(uint64_t eventId, const char* controllerName, float value)
    {
        m_runtime.UpdateEventController(eventId, controllerName, value);
    }
    void UpdateCustomEventRate(uint64_t eventId, float rateFactor)
    {
        m_runtime.UpdateEventRate(eventId, rateFactor);
    }
    void ServiceActiveEvents()
    {
        m_runtime.Service();
    }
    void RecordSpatialization(void* sample, void* route, const void* listener, const float* outputLevels, uint64_t outputChannelCount,
                              uint64_t inputChannelCount)
    {
        m_runtime.RecordSpatialization(sample, route, listener, outputLevels, outputChannelCount, inputChannelCount);
    }

  private:
    friend class CModAudioRuntime;

    struct RegexEventDefinition
    {
        std::regex Pattern;
        std::shared_ptr<ModAudioEventDefinition> Definition;
    };

    struct ActiveThreadState
    {
        std::shared_ptr<ClientSoundEventDefinition> ActiveClientSoundEvent;
        unsigned int ActiveClientDispatchDepth = 0;
        std::shared_ptr<ServerSoundAliasDefinition> ActiveServerSoundAlias;
    };

    static ActiveThreadState& GetActiveThreadState();
    void BeginClientSoundDispatch();
    void EndClientSoundDispatch();

    std::shared_ptr<ModAudioEventDefinition> FindExactDefinition(const char* eventId);
    std::shared_ptr<ClientSoundEventDefinition> FindClientSoundEventDefinition(const char* eventId);
    std::shared_ptr<ClientSoundEventDefinition> FindClientSoundEventDefinition(uint64_t eventHash);
    std::shared_ptr<ServerSoundAliasDefinition> FindServerSoundAliasDefinition(const char* eventId);

    std::shared_mutex m_registryMutex;
    std::unordered_map<std::string, std::shared_ptr<ModAudioEventDefinition>> m_eventDefinitions;
    std::unordered_map<std::string, RegexEventDefinition> m_regexEventDefinitions;
    std::unordered_map<std::string, std::shared_ptr<ClientSoundEventDefinition>> m_clientEventDefinitions;
    std::unordered_map<uint64_t, std::shared_ptr<ClientSoundEventDefinition>> m_clientEventDefinitionsByHash;
    std::unordered_map<std::string, std::shared_ptr<ServerSoundAliasDefinition>> m_serverAliasDefinitions;
    CModAudioRuntime m_runtime;
};

extern CModAudioManager g_ModAudioManager;
