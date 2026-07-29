#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

enum class AudioPlayResult
{
    NOT_CUSTOM,
    FAILED,
    HANDLED
};

enum class AudioSelectionStrategy
{
    SEQUENTIAL,
    RANDOM,
    RANDOM_NO_REPEAT,
    WEIGHTED_RANDOM
};

enum class AudioRouteMode
{
    DIRECT,
    PANNED,
    SPATIALIZED,
    MIXED
};

enum class AudioControllerSource
{
    GLOBAL,
    EVENT
};

enum class AudioControllerProperty
{
    VOLUME,
    PLAYBACK_RATE,
    LOW_PASS_CUTOFF,
    LFE_VOLUME,
    PAN_360,
    PAN_LEFT_RIGHT,
    PAN_FRONT_BACK,
    ROUTE_VOLUME,
    ROUTE_LFE_VOLUME,
    ROUTE_MATRIX,
    FILTER_PROPERTY,
    FILTER_WET,
    FILTER_DRY
};

enum class AudioControllerTarget
{
    SAMPLE,
    ROUTE,
    FILTER
};

enum class AudioSourceMode
{
    MEMORY,
    STREAM
};

enum class AudioSelectorMode
{
    SOURCE,
    SEQUENTIAL,
    RANDOM,
    RANDOM_NO_REPEAT,
    WEIGHTED_RANDOM,
    CONTROLLER
};

enum class AudioControllerOperation
{
    MULTIPLY,
    ADD,
    SET
};

enum class AudioInstanceLimitPolicy
{
    STEAL_OLDEST,
    REJECT_NEW
};

struct AudioGraphPoint
{
    float X = 0.0f;
    float Y = 0.0f;
    float IncomingTangentX = 0.0f;
    float IncomingTangentY = 0.0f;
    float OutgoingTangentX = 0.0f;
    float OutgoingTangentY = 0.0f;
    uint8_t IncomingType = 0;
    uint8_t OutgoingType = 0;
    uint8_t Padding[2] = {};
};

static_assert(sizeof(AudioGraphPoint) == 28);

struct AudioRouteDefinition
{
    std::string Bus;
    AudioRouteMode Mode = AudioRouteMode::PANNED;
    float Volume = 1.0f;
    float LFEVolume = 1.0f;
    std::vector<float> Matrix;
};

struct AudioControllerBinding
{
    std::string Controller;
    AudioControllerSource Source = AudioControllerSource::GLOBAL;
    AudioControllerTarget Target = AudioControllerTarget::SAMPLE;
    AudioControllerProperty Property = AudioControllerProperty::VOLUME;
    AudioControllerOperation Operation = AudioControllerOperation::MULTIPLY;
    unsigned int TargetIndex = 0;
    unsigned int MatrixIndex = 0;
    std::string PropertyName;
    float DefaultInput = 0.0f;
    std::vector<AudioGraphPoint> Curve;
};

struct AudioFilterPropertyDefinition
{
    std::string Name;
    float Value = 0.0f;
};

struct AudioFilterDefinition
{
    std::string Name;
    float Wet = 1.0f;
    float Dry = 0.0f;
    std::vector<AudioFilterPropertyDefinition> Properties;
};

struct AudioPanningDefinition
{
    bool HasPan360 = false;
    float Pan360Degrees = 0.0f;
    bool HasLeftRight = false;
    float LeftRight = 0.0f;
    bool HasFrontBack = false;
    float FrontBack = 0.0f;
    bool HasLevels = false;
    std::array<float, 5> Levels = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};

struct AudioSourceSelectorDefinition
{
    AudioSelectorMode Mode = AudioSelectorMode::SOURCE;
    std::string Source;
    size_t SourceIndex = std::numeric_limits<size_t>::max();
    float Weight = 1.0f;
    std::string Controller;
    AudioControllerSource ControllerSource = AudioControllerSource::EVENT;
    float DefaultInput = 0.0f;
    std::vector<AudioGraphPoint> Curve;
    std::vector<std::shared_ptr<AudioSourceSelectorDefinition>> Choices;
};

struct AudioSelectorRuntimeState
{
    size_t NextIndex = 0;
    std::vector<size_t> RemainingIndices;
};

struct AudioSampleData
{
    fs::path Path;
    size_t Size = 0;
    std::unique_ptr<uint8_t[]> Data;
    int DecoderType = 64;
};

struct AudioLayerDefinition
{
    std::string Name;
    std::shared_ptr<AudioSourceSelectorDefinition> Selector;
    std::vector<AudioRouteDefinition> Routes;
    std::vector<AudioControllerBinding> ControllerBindings;
    std::vector<AudioFilterDefinition> Filters;
    AudioPanningDefinition Panning;
    AudioSourceMode SourceMode = AudioSourceMode::MEMORY;
    unsigned int StreamBufferBytes = 262144;
    int PlayCount = 1;
    int LoopStartSamples = 0;
    int LoopEndSamples = -1;
    unsigned int StartPositionSamples = 0;
    unsigned int StartDelayMs = 0;
    float Volume = 1.0f;
    float VolumeRandomDb = 0.0f;
    float PlaybackRate = 1.0f;
    float PitchSemitones = 0.0f;
    float PitchRandomSemitones = 0.0f;
    float LowPassCutoff = -1.0f;
    float LFEVolume = 1.0f;
    float DopplerFactor = 0.0f;
    float MetersPerGameUnit = 0.01905f;
};

struct AudioSpatializationDefinition
{
    bool HasListenerMask = false;
    uint32_t ListenerMask = 0xFFFFFFFFu;
    bool HasAutoSpreadDistance = false;
    float AutoSpreadDistance = 0.0f;
    bool HasMultiChannelPan = false;
    float MultiChannelPanAngleDegrees = 0.0f;
    float MultiChannelPanDistance = 0.0f;
    bool HasOrientation = false;
    std::array<float, 3> Facing = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> Up = {0.0f, 0.0f, 1.0f};
    bool HasVolumeCone = false;
    bool VolumeConeEnabled = false;
    float VolumeConeInnerAngleDegrees = 360.0f;
    float VolumeConeOuterAngleDegrees = 360.0f;
    float VolumeConeOuterVolume = 1.0f;
    std::vector<AudioGraphPoint> VolumeCurve;
    std::vector<AudioGraphPoint> SpreadCurve;
    std::vector<AudioGraphPoint> LowPassCurve;
};

class ModAudioEventDefinition
{
  public:
    ModAudioEventDefinition(const std::string&, const fs::path&, const std::vector<std::string>& registeredEvents, bool loadAudioSamples);

    bool LoadedSuccessfully = false;
    std::vector<std::string> EventIds;
    std::vector<std::pair<std::string, std::regex>> EventIdsRegex;
    std::vector<AudioRouteDefinition> Routes;
    std::vector<AudioControllerBinding> ControllerBindings;
    std::vector<AudioLayerDefinition> Layers;
    AudioSpatializationDefinition Spatialization;
    int PlayCount = 1;
    int LoopStartSamples = 0;
    int LoopEndSamples = -1;
    float Volume = 1.0f;
    float PlaybackRate = 1.0f;
    float LowPassCutoff = 0.0f;
    unsigned int FadeInMs = 0;
    unsigned int FadeOutMs = 0;
    unsigned int MaxInstances = 0;
    AudioInstanceLimitPolicy InstanceLimitPolicy = AudioInstanceLimitPolicy::STEAL_OLDEST;
    uint32_t SoundTags = 0;
    float NetworkRadius = 0.0f;
    float NetworkDurationSeconds = 1.0f;
    std::vector<AudioSampleData> Samples;
    AudioSelectionStrategy Strategy = AudioSelectionStrategy::SEQUENTIAL;
    size_t CurrentIndex = 0;
    std::mutex SampleSelectionMutex;
    std::unordered_map<const AudioSourceSelectorDefinition*, AudioSelectorRuntimeState> SelectorStates;
    bool IsCustomEvent = false;
};

class ClientSoundEventDefinition
{
  public:
    explicit ClientSoundEventDefinition(std::string eventName);

    static uint64_t HashEventName(std::string_view eventName);

    void* GetClientDefinition()
    {
        return m_data.data();
    }
    const void* GetMilesEventDefinitionId() const
    {
        return m_data.data() + sizeof(uint64_t);
    }
    const std::string& GetEventName() const
    {
        return m_eventName;
    }

  private:
    alignas(8) std::array<uint8_t, 48> m_data{};
    std::string m_eventName;
};

// server.dll's sound digest stores one of these records for every networked
// alias. Custom events use the same record contract without modifying the
// fixed-size native digest table.
struct ServerSoundAliasDefinition
{
    ServerSoundAliasDefinition(const std::string& eventName, const ModAudioEventDefinition& eventDefinition)
        : EventHash(ClientSoundEventDefinition::HashEventName(eventName)), SoundTags(eventDefinition.SoundTags),
          NetworkRadius(eventDefinition.NetworkRadius), DurationSeconds(eventDefinition.NetworkDurationSeconds), EventName(eventName)
    {
    }

    uint64_t EventHash = 0;
    uint32_t SoundTags = 0;
    uint32_t NextEntryIndex = 0;
    float NetworkRadius = 0.0f;
    float DurationSeconds = 1.0f;
    std::string EventName;
};

static_assert(offsetof(ServerSoundAliasDefinition, EventHash) == 0x0);
static_assert(offsetof(ServerSoundAliasDefinition, SoundTags) == 0x8);
static_assert(offsetof(ServerSoundAliasDefinition, NextEntryIndex) == 0xC);
static_assert(offsetof(ServerSoundAliasDefinition, NetworkRadius) == 0x10);
static_assert(offsetof(ServerSoundAliasDefinition, DurationSeconds) == 0x14);
