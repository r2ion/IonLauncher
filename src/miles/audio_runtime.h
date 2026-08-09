#pragma once

#include "audio_types.h"

#include <array>
#include <chrono>
#include <limits>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using CustomMilesClock = std::chrono::steady_clock;

class CModAudioManager;

struct MilesSpatializationInfo
{
    std::array<float, 3> ListenerToSource;
    uint16_t AtListener;
    uint16_t Flags;
    float Gain;
    float NearFieldSpread;
    float Distance;
    float LowPassCutoff;
    float Spread;
    float SpreadMaxDistance;
};

static_assert(sizeof(MilesSpatializationInfo) == 0x28);

// Prefix of the native event context consumed by Northstar. Known fields end at
// byte 0x64; the C++ type includes four bytes of tail padding for pointer alignment.
struct MilesEventContextPrefix
{
    unsigned __int64 EventId;
    const char* EventName;
    const char* BankName;
    std::array<float, 3> FilterActorPosition;
    unsigned int FilterFlags;
    const float* ActorPosition;
    const float* ActorFacing;
    const float* ActorUp;
    std::array<uint8_t, 8> Unknown40;
    float RateFactor;
    unsigned int Unknown4C;
    const void* ControllerValues;
    const void* SelectionValues;
    unsigned int ListenerMask;
};

static_assert(sizeof(MilesEventContextPrefix) == 0x68);
static_assert(alignof(MilesEventContextPrefix) == 0x8);
static_assert(offsetof(MilesEventContextPrefix, EventId) == 0x0);
static_assert(offsetof(MilesEventContextPrefix, EventName) == 0x8);
static_assert(offsetof(MilesEventContextPrefix, BankName) == 0x10);
static_assert(offsetof(MilesEventContextPrefix, FilterActorPosition) == 0x18);
static_assert(offsetof(MilesEventContextPrefix, FilterFlags) == 0x24);
static_assert(offsetof(MilesEventContextPrefix, ActorPosition) == 0x28);
static_assert(offsetof(MilesEventContextPrefix, ActorFacing) == 0x30);
static_assert(offsetof(MilesEventContextPrefix, ActorUp) == 0x38);
static_assert(offsetof(MilesEventContextPrefix, Unknown40) == 0x40);
static_assert(offsetof(MilesEventContextPrefix, RateFactor) == 0x48);
static_assert(offsetof(MilesEventContextPrefix, ControllerValues) == 0x50);
static_assert(offsetof(MilesEventContextPrefix, Unknown4C) == 0x4C);
static_assert(offsetof(MilesEventContextPrefix, SelectionValues) == 0x58);
static_assert(offsetof(MilesEventContextPrefix, ListenerMask) == 0x60);
static_assert(offsetof(MilesEventContextPrefix, ListenerMask) + sizeof(decltype(MilesEventContextPrefix::ListenerMask)) == 0x64);

struct MilesQueuedControllerValue
{
    unsigned int Opcode;
    unsigned int Size;
    const MilesQueuedControllerValue* Previous;
    unsigned int ControllerIndex;
    float Value;
    char Name[1];
};

static_assert(offsetof(MilesQueuedControllerValue, Name) == 0x18);

struct ActiveModAudioLayer
{
    void* Sample = nullptr;
    const AudioLayerDefinition* Definition = nullptr;
    std::vector<void*> Routes;
    std::vector<void*> Filters;
    float RandomVolumeFactor = 1.0f;
    float RandomPitchFactor = 1.0f;
    float AppliedVolume = -1.0f;
    float AppliedPlaybackRate = -1.0f;
    float AppliedLowPassCutoff = -1.0f;
    float AppliedLFEVolume = -1.0f;
    float AppliedPan360 = std::numeric_limits<float>::quiet_NaN();
    float AppliedPanLeftRight = std::numeric_limits<float>::quiet_NaN();
    float AppliedPanFrontBack = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> AppliedRouteVolumes;
    std::vector<float> AppliedRouteLFEVolumes;
    std::vector<std::vector<float>> AppliedRouteMatrices;
    std::vector<float> AppliedFilterWet;
    std::vector<float> AppliedFilterDry;
    std::vector<std::unordered_map<std::string, float>> AppliedFilterProperties;
    bool LoggedSpatialization = false;
};

struct ActiveModAudioEvent
{
    void* EventSystem;
    std::shared_ptr<ModAudioEventDefinition> Definition;
    std::vector<ActiveModAudioLayer> Layers;
    std::string EventName;
    unsigned __int64 EventId;
    std::array<float, 3> ActorPosition;
    std::array<float, 3> ActorFacing = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> ActorUp = {0.0f, 0.0f, 1.0f};
    std::array<float, 3> SampleVelocity{};
    std::array<float, 3> ListenerVelocity{};
    std::array<float, 3> LastListenerPosition{};
    CustomMilesClock::time_point LastPositionUpdate = {};
    CustomMilesClock::time_point LastListenerUpdate = {};
    bool HasListenerPosition = false;
    std::unordered_map<std::string, float> EventControllerValues;
    bool Paused = false;
    bool Fading = false;
    bool DestroyAfterFade = false;
    float EventRateFactor = 1.0f;
    float FadeMultiplier = 1.0f;
    float FadeTargetMultiplier = 1.0f;
    std::chrono::duration<float> FadeRemaining = {};
    CustomMilesClock::time_point LastFadeUpdate = {};
};

struct ResolvedAudioRoute
{
    const AudioRouteDefinition* Definition;
    void* RuntimeBus;
};

enum class CustomMilesControl
{
    NONE,
    STOP_NOW,
    STOP,
    PAUSE,
    RESUME
};

class CModAudioRuntime
{
  public:
    explicit CModAudioRuntime(CModAudioManager& manager) : m_manager(manager)
    {
    }

    AudioPlayResult TryPlayEvent(void* eventSystem, const char* eventName, uint64_t eventId, const void* eventContext);
    void ApplyEventControl(const char* controlName, const void* eventContext);
    void UpdateEventPosition(uint64_t eventId, float x, float y, float z);
    void UpdateEventOrientation(uint64_t eventId, float facingX, float facingY, float facingZ, float upX, float upY, float upZ);
    void UpdateEventController(uint64_t eventId, const char* controllerName, float value);
    void UpdateEventRate(uint64_t eventId, float rateFactor);
    void RecordSpatialization(void* sample, void* route, const void* listener, const float* outputLevels, uint64_t outputChannelCount,
                              uint64_t inputChannelCount);
    bool TryGetReplacementSample(const char* eventName, void*& data, unsigned int& dataLength, int& decoderType);
    bool IsControlName(const char* eventName) const;

    void Service();
    void StopAll();

  private:
    using EventIterator = std::vector<ActiveModAudioEvent>::iterator;

    EventIterator Destroy(EventIterator event);
    static bool IsSampleActive(unsigned int status);
    static float EvaluateGraph(const std::vector<AudioGraphPoint>& curve, float input);
    static void ApplyControllerOperation(float& property, float value, AudioControllerOperation operation);
    static void ApplyProperties(ActiveModAudioEvent& instance);
    static void BeginFade(ActiveModAudioEvent& instance, float targetMultiplier, unsigned int durationMs, bool destroyAfterFade);
    static std::mt19937& RandomGenerator();
    static size_t SelectRandomIndex(size_t count);
    static bool SelectAudioSample(const std::shared_ptr<ModAudioEventDefinition>& definition, void*& data, unsigned int& dataLength,
                                  int& decoderType);
    static const AudioSampleData* SelectLayerAudioSourceLocked(ModAudioEventDefinition& eventDefinition,
                                                               const std::shared_ptr<AudioSourceSelectorDefinition>& selector, void* eventSystem,
                                                               const std::unordered_map<std::string, float>& eventControllerValues);
    static const AudioSampleData* SelectLayerAudioSource(ModAudioEventDefinition& eventDefinition, const AudioLayerDefinition& layer,
                                                         void* eventSystem, const std::unordered_map<std::string, float>& eventControllerValues);
    static bool ShouldPlayAudioEvent(const char* eventName, const std::shared_ptr<ModAudioEventDefinition>& definition);
    static CustomMilesControl GetControl(const char* eventName);
    static bool EventNameMatches(const ActiveModAudioEvent& instance, const MilesEventContextPrefix& context);
    static bool InstanceMatches(const ActiveModAudioEvent& instance, const MilesEventContextPrefix* context);
    static bool ConfigureRoute(void* route, const AudioRouteDefinition& definition, const char* eventName);
    static void FinalizeSpatialization(void* sample, MilesSpatializationInfo* spatializationInfo);
    static bool ConfigureSpatialization(void* sample, const ModAudioEventDefinition& definition, const std::array<float, 3>& actorPosition,
                                        const std::array<float, 3>& actorFacing, const std::array<float, 3>& actorUp, unsigned int listenerMask,
                                        const char* eventName);
    static bool ValidateSelectorControllers(void* eventSystem, const std::shared_ptr<AudioSourceSelectorDefinition>& selector, const char* eventName);

    CModAudioManager& m_manager;
    std::mutex m_mutex;
    std::vector<ActiveModAudioEvent> m_events;
};
