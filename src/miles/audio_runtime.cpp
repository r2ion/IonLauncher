#include "audio.h"
#include "core/convar/convar.h"
#include "dedicated/dedicated.h"
#include "logging/logging.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <mutex>
#include <numeric>
#include <random>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

DECLARE_MODULE(AudioHooks)

static thread_local const char* pszAudioEventName;
static thread_local bool s_loadingCustomMilesSample;

ConVar* Cvar_mileslog_enable;
ConVar* Cvar_ns_print_played_sounds;

// Empty stereo 48000 WAVE file used when a replacement intentionally has no
// matching sample for the current event.
static unsigned char EMPTY_WAVE[45] = {0x52, 0x49, 0x46, 0x46, 0x25, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74,
                                       0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x44, 0xAC, 0x00, 0x00, 0x88, 0x58,
                                       0x01, 0x00, 0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x74, 0x00, 0x00, 0x00, 0x00};

typedef void (*MilesStopAll_Type)();
MilesStopAll_Type MilesStopAll;

using MilesQueueEventRun_Type = signed __int64 (*)(void* queue, const char* eventName);
using MilesEvaluateGraph_Type = float (*)(const AudioGraphPoint* points, int pointCount, float input);
using MilesIntBusLookup_Type = void* (*)(void* eventSystem, const char* busName);
using MilesIntControllerIndex_Type = unsigned int (*)(void* eventSystem, const char* controllerName);
using MilesIntControllerGet_Type = float (*)(void* eventSystem, void* voice, const char* controllerName);
using MilesSampleCreate_Type = void* (*)(void* driver, void* primaryOutput, void* route);
using MilesSampleDestroy_Type = void (*)(void* sample);
using MilesSampleGetStatus_Type = unsigned int (*)(void* sample);
using MilesSampleSetSource_Type = bool (*)(void* sample, const void* data, unsigned int dataSize, int format);
using MilesSampleSetSourceStream_Type = bool (*)(void* sample, const char* fileName, unsigned int streamBufferBytes, const void* preloadData,
                                                 unsigned int preloadBytes);
using MilesSampleCreateRoute_Type = void* (*)(void* sample, void* output, float volume, void* route);
using MilesSampleGetRoute_Type = void* (*)(void* sample, int index);
using MilesSamplePlay_Type = void (*)(void* sample);
using MilesSamplePlayScheduled_Type = void (*)(void* sample, unsigned __int64 mixSample);
using MilesSamplePause_Type = void (*)(void* sample);
using MilesSampleSetPlayCount_Type = void (*)(void* sample, int playCount);
using MilesSampleSetLoopSamples_Type = void (*)(void* sample, int startSample, int endSample);
using MilesSampleGetDurationSamples_Type = unsigned int (*)(void* sample);
using MilesSampleSetPositionSamples_Type = void (*)(void* sample, unsigned int positionSamples);
using MilesSampleSetVolumeLevel_Type = void (*)(void* sample, float volume);
using MilesSampleSetLFEVolumeLevel_Type = void (*)(void* sample, float volume);
using MilesSampleSetPlaybackRateFactor_Type = void (*)(void* sample, float rate);
using MilesSampleSetLowPassCutoff_Type = void (*)(void* sample, float cutoff);
using MilesSampleSetPan_Type = void (*)(void* sample, float value);
using MilesSampleSetPanLevels_Type = void (*)(void* sample, float front, float back, float left, float right, float center);
using MilesSampleSetDopplerFactor_Type = void (*)(void* sample, float effectiveness, const float* listenerVelocity, const float* sampleVelocity,
                                                  float metersPerGameUnit);
using MilesSampleSetListenerMask_Type = void (*)(void* sample, unsigned int listenerMask);
using MilesSampleSet3DPosition_Type = void (*)(void* sample, float x, float y, float z);
using MilesSampleSet3DOrientation_Type = void (*)(void* sample, float facingX, float facingY, float facingZ, float upX, float upY, float upZ);
using MilesSampleSet3DAutoSpreadDistance_Type = void (*)(void* sample, float distance);
using MilesSampleSet3DMultiChannelPan_Type = void (*)(void* sample, float angleDegrees, float distance);
using MilesSampleSet3DGraph_Type = void (*)(void* sample, const AudioGraphPoint* points, unsigned int pointCount);
using MilesSampleSet3DVolumeCone_Type = void (*)(void* sample, int enabled, float innerAngleDegrees, float outerAngleDegrees, float outerVolume);
using MilesSample3DUserFalloffFn_Type = void (*)(void* sample, MilesSpatializationInfo* spatializationInfo);
using MilesSampleSet3DUserFalloffFn_Type = void (*)(void* sample, MilesSample3DUserFalloffFn_Type callback);
using MilesRouteSetMode_Type = void (*)(void* route);
using MilesRouteSetSpatialized_Type = void (*)(void* route, void* output);
using MilesRouteSetVolumeLevel_Type = void (*)(void* route, float volume);
using MilesRouteSetLFELevel_Type = void (*)(void* route, float volume);
using MilesRouteSetMixed_Type = void (*)(void* route, const float* matrix);
using MilesSampleAddFilterByName_Type = void* (*)(void* sample, const char* filterName);
using MilesFilterSetPropertyValueByName_Type = bool (*)(void* filter, const char* propertyName, float value);
using MilesFilterSetWetDryLevels_Type = void (*)(void* filter, float wet, float dry);
using MilesDriverGetMixedSamples_Type = unsigned __int64 (*)(void* driver);
using MilesDriverGetSampleRate_Type = unsigned int (*)(void* driver);

static MilesQueueEventRun_Type MilesQueueEventRun;
static MilesEvaluateGraph_Type MilesEvaluateGraph;
static MilesIntBusLookup_Type MilesIntBusLookup;
static MilesIntControllerIndex_Type MilesIntControllerIndex;
static MilesIntControllerGet_Type MilesIntControllerGet;
static MilesSampleCreate_Type MilesSampleCreate;
static MilesSampleDestroy_Type MilesSampleDestroy;
static MilesSampleGetStatus_Type MilesSampleGetStatus;
static MilesSampleSetSource_Type MilesSampleSetSource;
static MilesSampleSetSourceStream_Type MilesSampleSetSourceStream;
static MilesSampleCreateRoute_Type MilesSampleCreateRoute;
static MilesSampleGetRoute_Type MilesSampleGetRoute;
static MilesSamplePlay_Type MilesSamplePlay;
static MilesSamplePlayScheduled_Type MilesSamplePlayScheduled;
static MilesSamplePause_Type MilesSamplePause;
static MilesSampleSetPlayCount_Type MilesSampleSetPlayCount;
static MilesSampleSetLoopSamples_Type MilesSampleSetLoopSamples;
static MilesSampleGetDurationSamples_Type MilesSampleGetDurationSamples;
static MilesSampleSetPositionSamples_Type MilesSampleSetPositionSamples;
static MilesSampleSetVolumeLevel_Type MilesSampleSetVolumeLevel;
static MilesSampleSetLFEVolumeLevel_Type MilesSampleSetLFEVolumeLevel;
static MilesSampleSetPlaybackRateFactor_Type MilesSampleSetPlaybackRateFactor;
static MilesSampleSetLowPassCutoff_Type MilesSampleSetLowPassCutoff;
static MilesSampleSetPan_Type MilesSampleSetPan360;
static MilesSampleSetPan_Type MilesSampleSetPanLeftRight;
static MilesSampleSetPan_Type MilesSampleSetPanFrontBack;
static MilesSampleSetPanLevels_Type MilesSampleSetPanLevels;
static MilesSampleSetDopplerFactor_Type MilesSampleSetDopplerFactor;
static MilesSampleSetListenerMask_Type MilesSampleSetListenerMask;
static MilesSampleSet3DPosition_Type MilesSampleSet3DPosition;
static MilesSampleSet3DOrientation_Type MilesSampleSet3DOrientation;
static MilesSampleSet3DAutoSpreadDistance_Type MilesSampleSet3DAutoSpreadDistance;
static MilesSampleSet3DMultiChannelPan_Type MilesSampleSet3DMultiChannelPan;
static MilesSampleSet3DGraph_Type MilesSampleSet3DSpreadGraph;
static MilesSampleSet3DGraph_Type MilesSampleSet3DLowPassGraph;
static MilesSampleSet3DGraph_Type MilesSampleSet3DVolumeGraph;
static MilesSampleSet3DVolumeCone_Type MilesSampleSet3DVolumeCone;
static MilesSampleSet3DUserFalloffFn_Type MilesSampleSet3DUserFalloffFn;
static MilesRouteSetMode_Type MilesRouteSetDirect;
static MilesRouteSetMode_Type MilesRouteSetPanned;
static MilesRouteSetSpatialized_Type MilesRouteSetSpatialized;
static MilesRouteSetVolumeLevel_Type MilesRouteSetVolumeLevel;
static MilesRouteSetLFELevel_Type MilesRouteSetLFELevel;
static MilesRouteSetMixed_Type MilesRouteSetMixed;
static MilesSampleAddFilterByName_Type MilesSampleAddFilterByName;
static MilesFilterSetPropertyValueByName_Type MilesFilterSetPropertyValueByName;
static MilesFilterSetWetDryLevels_Type MilesFilterSetWetDryLevels;
static MilesDriverGetMixedSamples_Type MilesDriverGetMixedSamples;
static MilesDriverGetSampleRate_Type MilesDriverGetSampleRate;

bool CModAudioRuntime::IsSampleActive(unsigned int status)
{
    return status == 2 || status == 3 || status == 4 || status == 5;
}

float CModAudioRuntime::EvaluateGraph(const std::vector<AudioGraphPoint>& curve, float input)
{
    if (MilesEvaluateGraph)
        return MilesEvaluateGraph(curve.data(), static_cast<int>(curve.size()), input);

    if (input <= curve.front().X)
        return curve.front().Y;
    if (input >= curve.back().X)
        return curve.back().Y;

    const auto upper = std::upper_bound(curve.begin(), curve.end(), input, [](float value, const AudioGraphPoint& point) { return value < point.X; });
    const AudioGraphPoint& right = *upper;
    const AudioGraphPoint& left = *(upper - 1);
    const float fraction = (input - left.X) / (right.X - left.X);
    return left.Y + ((right.Y - left.Y) * fraction);
}

void CModAudioRuntime::ApplyControllerOperation(float& property, float value, AudioControllerOperation operation)
{
    switch (operation)
    {
    case AudioControllerOperation::MULTIPLY:
        property *= value;
        break;
    case AudioControllerOperation::ADD:
        property += value;
        break;
    case AudioControllerOperation::SET:
        property = value;
        break;
    }
}

void CModAudioRuntime::ApplyProperties(ActiveModAudioEvent& instance)
{
    auto controllerInput = [&](const AudioControllerBinding& binding)
    {
        float input = binding.DefaultInput;
        if (binding.Source == AudioControllerSource::GLOBAL)
            input = MilesIntControllerGet(instance.EventSystem, nullptr, binding.Controller.c_str());
        else
        {
            auto value = instance.EventControllerValues.find(binding.Controller);
            if (value != instance.EventControllerValues.end())
                input = value->second;
        }
        return EvaluateGraph(binding.Curve, input);
    };

    for (ActiveModAudioLayer& layer : instance.Layers)
    {
        if (!layer.Sample || !layer.Definition)
            continue;
        const AudioLayerDefinition& definition = *layer.Definition;
        float volume = instance.Definition->Volume * definition.Volume * layer.RandomVolumeFactor;
        float playbackRate = instance.Definition->PlaybackRate * definition.PlaybackRate * layer.RandomPitchFactor * instance.EventRateFactor;
        float lowPassCutoff = definition.LowPassCutoff >= 0.0f ? definition.LowPassCutoff : instance.Definition->LowPassCutoff;
        float lfeVolume = definition.LFEVolume;
        float pan360 = definition.Panning.Pan360Degrees;
        float panLeftRight = definition.Panning.LeftRight;
        float panFrontBack = definition.Panning.FrontBack;
        bool hasPan360 = definition.Panning.HasPan360;
        bool hasPanLeftRight = definition.Panning.HasLeftRight;
        bool hasPanFrontBack = definition.Panning.HasFrontBack;

        std::vector<float> routeVolumes;
        std::vector<float> routeLFEVolumes;
        std::vector<std::vector<float>> routeMatrices;
        for (const AudioRouteDefinition& route : definition.Routes)
        {
            routeVolumes.push_back(route.Volume);
            routeLFEVolumes.push_back(route.LFEVolume);
            routeMatrices.push_back(route.Matrix);
        }
        std::vector<float> filterWet;
        std::vector<float> filterDry;
        std::vector<std::unordered_map<std::string, float>> filterProperties;
        for (const AudioFilterDefinition& filter : definition.Filters)
        {
            filterWet.push_back(filter.Wet);
            filterDry.push_back(filter.Dry);
            std::unordered_map<std::string, float> properties;
            for (const AudioFilterPropertyDefinition& property : filter.Properties)
                properties[property.Name] = property.Value;
            filterProperties.push_back(std::move(properties));
        }

        auto applyBinding = [&](const AudioControllerBinding& binding)
        {
            const float output = controllerInput(binding);
            if (binding.Target == AudioControllerTarget::SAMPLE)
            {
                switch (binding.Property)
                {
                case AudioControllerProperty::VOLUME:
                    ApplyControllerOperation(volume, output, binding.Operation);
                    break;
                case AudioControllerProperty::PLAYBACK_RATE:
                    ApplyControllerOperation(playbackRate, output, binding.Operation);
                    break;
                case AudioControllerProperty::LOW_PASS_CUTOFF:
                    ApplyControllerOperation(lowPassCutoff, output, binding.Operation);
                    break;
                case AudioControllerProperty::LFE_VOLUME:
                    ApplyControllerOperation(lfeVolume, output, binding.Operation);
                    break;
                case AudioControllerProperty::PAN_360:
                    hasPan360 = true;
                    ApplyControllerOperation(pan360, output, binding.Operation);
                    break;
                case AudioControllerProperty::PAN_LEFT_RIGHT:
                    hasPanLeftRight = true;
                    ApplyControllerOperation(panLeftRight, output, binding.Operation);
                    break;
                case AudioControllerProperty::PAN_FRONT_BACK:
                    hasPanFrontBack = true;
                    ApplyControllerOperation(panFrontBack, output, binding.Operation);
                    break;
                default:
                    break;
                }
            }
            else if (binding.Target == AudioControllerTarget::ROUTE && binding.TargetIndex < routeVolumes.size())
            {
                if (binding.Property == AudioControllerProperty::ROUTE_VOLUME)
                    ApplyControllerOperation(routeVolumes[binding.TargetIndex], output, binding.Operation);
                else if (binding.Property == AudioControllerProperty::ROUTE_LFE_VOLUME)
                    ApplyControllerOperation(routeLFEVolumes[binding.TargetIndex], output, binding.Operation);
                else if (binding.Property == AudioControllerProperty::ROUTE_MATRIX && binding.MatrixIndex < routeMatrices[binding.TargetIndex].size())
                    ApplyControllerOperation(routeMatrices[binding.TargetIndex][binding.MatrixIndex], output, binding.Operation);
            }
            else if (binding.Target == AudioControllerTarget::FILTER && binding.TargetIndex < filterWet.size())
            {
                if (binding.Property == AudioControllerProperty::FILTER_WET)
                    ApplyControllerOperation(filterWet[binding.TargetIndex], output, binding.Operation);
                else if (binding.Property == AudioControllerProperty::FILTER_DRY)
                    ApplyControllerOperation(filterDry[binding.TargetIndex], output, binding.Operation);
                else if (binding.Property == AudioControllerProperty::FILTER_PROPERTY)
                    ApplyControllerOperation(filterProperties[binding.TargetIndex][binding.PropertyName], output, binding.Operation);
            }
        };
        for (const AudioControllerBinding& binding : instance.Definition->ControllerBindings)
            applyBinding(binding);
        for (const AudioControllerBinding& binding : definition.ControllerBindings)
            applyBinding(binding);

        volume = std::max(volume * instance.FadeMultiplier, 0.0f);
        playbackRate = std::max(playbackRate, 0.01f);
        lowPassCutoff = std::clamp(lowPassCutoff, 0.0f, 20000.0f);
        lfeVolume = std::max(lfeVolume, 0.0f);

        if (MilesSampleSetVolumeLevel && volume != layer.AppliedVolume)
        {
            MilesSampleSetVolumeLevel(layer.Sample, volume);
            layer.AppliedVolume = volume;
        }
        if (MilesSampleSetPlaybackRateFactor && playbackRate != layer.AppliedPlaybackRate)
        {
            MilesSampleSetPlaybackRateFactor(layer.Sample, playbackRate);
            layer.AppliedPlaybackRate = playbackRate;
        }
        if (MilesSampleSetLowPassCutoff && lowPassCutoff != layer.AppliedLowPassCutoff)
        {
            MilesSampleSetLowPassCutoff(layer.Sample, lowPassCutoff);
            layer.AppliedLowPassCutoff = lowPassCutoff;
        }
        if (MilesSampleSetLFEVolumeLevel && lfeVolume != layer.AppliedLFEVolume)
        {
            MilesSampleSetLFEVolumeLevel(layer.Sample, lfeVolume);
            layer.AppliedLFEVolume = lfeVolume;
        }
        if (hasPan360 && MilesSampleSetPan360 && pan360 != layer.AppliedPan360)
        {
            MilesSampleSetPan360(layer.Sample, pan360);
            layer.AppliedPan360 = pan360;
        }
        if (hasPanLeftRight && MilesSampleSetPanLeftRight && panLeftRight != layer.AppliedPanLeftRight)
        {
            MilesSampleSetPanLeftRight(layer.Sample, panLeftRight);
            layer.AppliedPanLeftRight = panLeftRight;
        }
        if (hasPanFrontBack && MilesSampleSetPanFrontBack && panFrontBack != layer.AppliedPanFrontBack)
        {
            MilesSampleSetPanFrontBack(layer.Sample, panFrontBack);
            layer.AppliedPanFrontBack = panFrontBack;
        }

        for (size_t index = 0; index < layer.Routes.size() && index < routeVolumes.size(); ++index)
        {
            if (MilesRouteSetVolumeLevel && routeVolumes[index] != layer.AppliedRouteVolumes[index])
            {
                MilesRouteSetVolumeLevel(layer.Routes[index], std::max(routeVolumes[index], 0.0f));
                layer.AppliedRouteVolumes[index] = routeVolumes[index];
            }
            if (MilesRouteSetLFELevel && routeLFEVolumes[index] != layer.AppliedRouteLFEVolumes[index])
            {
                MilesRouteSetLFELevel(layer.Routes[index], std::max(routeLFEVolumes[index], 0.0f));
                layer.AppliedRouteLFEVolumes[index] = routeLFEVolumes[index];
            }
            if (!routeMatrices[index].empty() && MilesRouteSetMixed && routeMatrices[index] != layer.AppliedRouteMatrices[index])
            {
                MilesRouteSetMixed(layer.Routes[index], routeMatrices[index].data());
                layer.AppliedRouteMatrices[index] = routeMatrices[index];
            }
        }
        for (size_t index = 0; index < layer.Filters.size() && index < filterWet.size(); ++index)
        {
            if (MilesFilterSetWetDryLevels &&
                (filterWet[index] != layer.AppliedFilterWet[index] || filterDry[index] != layer.AppliedFilterDry[index]))
            {
                MilesFilterSetWetDryLevels(layer.Filters[index], std::max(filterWet[index], 0.0f), std::max(filterDry[index], 0.0f));
                layer.AppliedFilterWet[index] = filterWet[index];
                layer.AppliedFilterDry[index] = filterDry[index];
            }
            if (MilesFilterSetPropertyValueByName)
            {
                for (const auto& [name, value] : filterProperties[index])
                {
                    auto applied = layer.AppliedFilterProperties[index].find(name);
                    if (applied == layer.AppliedFilterProperties[index].end() || applied->second != value)
                    {
                        if (!MilesFilterSetPropertyValueByName(layer.Filters[index], name.c_str(), value))
                        {
                            NS::log::MILES->error("Could not set Miles filter property {} on custom event {} layer {} filter {}", name,
                                                  instance.EventName, definition.Name, definition.Filters[index].Name);
                        }
                        layer.AppliedFilterProperties[index][name] = value;
                    }
                }
            }
        }
    }
}

CModAudioRuntime::EventIterator CModAudioRuntime::Destroy(EventIterator event)
{
    for (ActiveModAudioLayer& layer : event->Layers)
    {
        if (!layer.Sample)
            continue;
        if (MilesSampleGetStatus && MilesSamplePause && IsSampleActive(MilesSampleGetStatus(layer.Sample)))
            MilesSamplePause(layer.Sample);
        if (MilesSampleDestroy)
            MilesSampleDestroy(layer.Sample);
        layer.Sample = nullptr;
    }

    return m_events.erase(event);
}

void CModAudioRuntime::BeginFade(ActiveModAudioEvent& instance, float targetMultiplier, unsigned int durationMs, bool destroyAfterFade)
{
    instance.FadeTargetMultiplier = targetMultiplier;
    instance.DestroyAfterFade = destroyAfterFade;
    instance.LastFadeUpdate = CustomMilesClock::now();
    instance.FadeRemaining = std::chrono::duration<float>(std::chrono::milliseconds(durationMs));
    instance.Fading = durationMs != 0 && instance.FadeMultiplier != targetMultiplier;

    if (!instance.Fading)
    {
        instance.FadeMultiplier = targetMultiplier;
        ApplyProperties(instance);
    }
}

void CModAudioRuntime::Service()
{
    if (!MilesSampleDestroy || !MilesSampleGetStatus)
        return;

    const CustomMilesClock::time_point now = CustomMilesClock::now();
    // Miles invokes this hook while its recursive audio lock may still be held.
    // Never wait on mod-reload cleanup, which takes this mutex before calling
    // public Miles sample APIs.
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    auto iter = m_events.begin();
    while (iter != m_events.end())
    {
        bool hasActiveLayer = false;
        for (ActiveModAudioLayer& layer : iter->Layers)
        {
            if (!layer.Sample)
                continue;
            if (IsSampleActive(MilesSampleGetStatus(layer.Sample)))
            {
                hasActiveLayer = true;
                continue;
            }
            MilesSampleDestroy(layer.Sample);
            layer.Sample = nullptr;
        }
        if (!hasActiveLayer)
        {
            iter = Destroy(iter);
            continue;
        }

        if (iter->Fading && !iter->Paused)
        {
            const std::chrono::duration<float> elapsed = now - iter->LastFadeUpdate;
            iter->LastFadeUpdate = now;

            if (elapsed >= iter->FadeRemaining)
            {
                iter->FadeMultiplier = iter->FadeTargetMultiplier;
                iter->FadeRemaining = {};
                iter->Fading = false;
            }
            else
            {
                const float fraction = elapsed.count() / iter->FadeRemaining.count();
                iter->FadeMultiplier += (iter->FadeTargetMultiplier - iter->FadeMultiplier) * fraction;
                iter->FadeRemaining -= elapsed;
            }
        }

        if (!iter->Fading && iter->DestroyAfterFade)
        {
            iter = Destroy(iter);
            continue;
        }

        if (!iter->Paused)
        {
            if (now - iter->LastPositionUpdate > std::chrono::milliseconds(100))
                iter->SampleVelocity.fill(0.0f);
            if (now - iter->LastListenerUpdate > std::chrono::milliseconds(100))
                iter->ListenerVelocity.fill(0.0f);
            if (MilesSampleSetDopplerFactor)
            {
                for (ActiveModAudioLayer& layer : iter->Layers)
                {
                    if (layer.Sample && layer.Definition && layer.Definition->DopplerFactor > 0.0f)
                    {
                        MilesSampleSetDopplerFactor(layer.Sample, layer.Definition->DopplerFactor, iter->ListenerVelocity.data(),
                                                    iter->SampleVelocity.data(), layer.Definition->MetersPerGameUnit);
                    }
                }
            }
            ApplyProperties(*iter);
        }

        ++iter;
    }
}

void CModAudioRuntime::StopAll()
{
    if (!MilesSampleDestroy)
        return;

    std::lock_guard lock(m_mutex);
    auto iter = m_events.begin();
    while (iter != m_events.end())
        iter = Destroy(iter);
}

void CModAudioManager::Clear()
{
    bool hasOverrides = false;
    {
        std::shared_lock lock(m_registryMutex);
        hasOverrides = !m_eventDefinitions.empty() || !m_regexEventDefinitions.empty();
    }

    if (!IsDedicatedServer() && hasOverrides)
    {
        MilesStopAll();
        Sleep(50);
    }

    if (!IsDedicatedServer())
        m_runtime.StopAll();

    GetActiveThreadState() = {};

    std::unique_lock lock(m_registryMutex);
    m_eventDefinitions.clear();
    m_regexEventDefinitions.clear();
    m_clientEventDefinitions.clear();
    m_clientEventDefinitionsByHash.clear();
    m_serverAliasDefinitions.clear();
}

std::mt19937& CModAudioRuntime::RandomGenerator()
{
    static thread_local std::mt19937 generator(std::random_device{}());
    return generator;
}

size_t CModAudioRuntime::SelectRandomIndex(size_t count)
{
    std::uniform_int_distribution<size_t> distribution(0, count - 1);
    return distribution(RandomGenerator());
}

bool CModAudioRuntime::SelectAudioSample(const std::shared_ptr<ModAudioEventDefinition>& definition, void*& data, unsigned int& dataLength)
{
    std::lock_guard lock(definition->SampleSelectionMutex);

    if (definition->Samples.empty())
    {
        data = EMPTY_WAVE;
        dataLength = sizeof(EMPTY_WAVE);
        return true;
    }

    AudioSampleData* sample = nullptr;
    switch (definition->Strategy)
    {
    case AudioSelectionStrategy::RANDOM:
    case AudioSelectionStrategy::RANDOM_NO_REPEAT:
    case AudioSelectionStrategy::WEIGHTED_RANDOM:
        sample = &definition->Samples[SelectRandomIndex(definition->Samples.size())];
        break;
    case AudioSelectionStrategy::SEQUENTIAL:
    default:
        sample = &definition->Samples[definition->CurrentIndex++];
        if (definition->CurrentIndex >= definition->Samples.size())
            definition->CurrentIndex = 0;
        break;
    }

    if (!sample)
        return false;

    data = sample->Data.get();
    dataLength = static_cast<unsigned int>(sample->Size);
    return data != nullptr;
}

const AudioSampleData* CModAudioRuntime::SelectLayerAudioSourceLocked(ModAudioEventDefinition& eventDefinition,
                                                                      const std::shared_ptr<AudioSourceSelectorDefinition>& selector,
                                                                      void* eventSystem,
                                                                      const std::unordered_map<std::string, float>& eventControllerValues)
{
    if (!selector)
        return nullptr;
    if (selector->Mode == AudioSelectorMode::SOURCE)
        return selector->SourceIndex < eventDefinition.Samples.size() ? &eventDefinition.Samples[selector->SourceIndex] : nullptr;
    if (selector->Choices.empty())
        return nullptr;

    size_t choiceIndex = 0;
    AudioSelectorRuntimeState& state = eventDefinition.SelectorStates[selector.get()];
    switch (selector->Mode)
    {
    case AudioSelectorMode::SEQUENTIAL:
        choiceIndex = state.NextIndex++ % selector->Choices.size();
        break;
    case AudioSelectorMode::RANDOM:
    {
        choiceIndex = SelectRandomIndex(selector->Choices.size());
        break;
    }
    case AudioSelectorMode::RANDOM_NO_REPEAT:
    {
        if (state.RemainingIndices.empty())
        {
            state.RemainingIndices.resize(selector->Choices.size());
            std::iota(state.RemainingIndices.begin(), state.RemainingIndices.end(), 0);
        }
        const size_t remainingIndex = SelectRandomIndex(state.RemainingIndices.size());
        choiceIndex = state.RemainingIndices[remainingIndex];
        state.RemainingIndices.erase(state.RemainingIndices.begin() + remainingIndex);
        break;
    }
    case AudioSelectorMode::WEIGHTED_RANDOM:
    {
        std::vector<double> weights;
        weights.reserve(selector->Choices.size());
        for (const std::shared_ptr<AudioSourceSelectorDefinition>& choice : selector->Choices)
            weights.push_back(std::max(static_cast<double>(choice->Weight), 0.0));
        std::discrete_distribution<size_t> distribution(weights.begin(), weights.end());
        choiceIndex = distribution(RandomGenerator());
        break;
    }
    case AudioSelectorMode::CONTROLLER:
    {
        float input = selector->DefaultInput;
        if (selector->ControllerSource == AudioControllerSource::GLOBAL)
            input = MilesIntControllerGet(eventSystem, nullptr, selector->Controller.c_str());
        else
        {
            auto value = eventControllerValues.find(selector->Controller);
            if (value != eventControllerValues.end())
                input = value->second;
        }
        const float selected = selector->Curve.empty() ? input : EvaluateGraph(selector->Curve, input);
        choiceIndex = static_cast<size_t>(std::clamp(static_cast<int>(std::floor(selected)), 0, static_cast<int>(selector->Choices.size() - 1)));
        break;
    }
    case AudioSelectorMode::SOURCE:
    default:
        break;
    }
    return SelectLayerAudioSourceLocked(eventDefinition, selector->Choices[choiceIndex], eventSystem, eventControllerValues);
}

const AudioSampleData* CModAudioRuntime::SelectLayerAudioSource(ModAudioEventDefinition& eventDefinition, const AudioLayerDefinition& layer,
                                                                void* eventSystem,
                                                                const std::unordered_map<std::string, float>& eventControllerValues)
{
    std::lock_guard lock(eventDefinition.SampleSelectionMutex);
    return SelectLayerAudioSourceLocked(eventDefinition, layer.Selector, eventSystem, eventControllerValues);
}

bool CModAudioRuntime::ShouldPlayAudioEvent(const char* eventName, const std::shared_ptr<ModAudioEventDefinition>& definition)
{
    std::string eventNameString = eventName;
    std::string eventNameStringBlacklistEntry = ("!" + eventNameString);

    for (const std::string& name : definition->EventIds)
    {
        if (name == eventNameStringBlacklistEntry)
            return false; // event blacklisted

        if (name == "*")
        {
            // check for bad sounds I guess?
            // really feel like this should be an option but whatever
            if (!!strstr(eventName, "_amb_") || !!strstr(eventName, "_emit_") || !!strstr(eventName, "amb_"))
                return false; // would play static noise, I hate this
        }
    }

    return true; // good to go
}

bool CModAudioRuntime::TryGetReplacementSample(const char* eventName, void*& data, unsigned int& dataLength)
{
    std::shared_ptr<ModAudioEventDefinition> definition = m_manager.FindReplacementDefinition(eventName);
    if (!definition || !ShouldPlayAudioEvent(eventName, definition))
        return false;

    if (!SelectAudioSample(definition, data, dataLength))
        NS::log::MILES->warn("Could not get sample data from override definition for event {}", eventName);
    if (data)
        return true;

    NS::log::MILES->warn("Could not fetch override sample data for event {}! Using original data instead.", eventName);
    return false;
}

using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE hThread, PCWSTR lpThreadDescription);
static SetThreadDescriptionFn pSetThreadDescription =
    reinterpret_cast<SetThreadDescriptionFn>(GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetThreadDescription"));

CustomMilesControl CModAudioRuntime::GetControl(const char* eventName)
{
    if (!eventName)
        return CustomMilesControl::NONE;
    if (strcmp(eventName, "STOPNOW") == 0)
        return CustomMilesControl::STOP_NOW;
    if (strcmp(eventName, "STOP") == 0)
        return CustomMilesControl::STOP;
    if (strcmp(eventName, "PAUSE") == 0)
        return CustomMilesControl::PAUSE;
    if (strcmp(eventName, "RESUME") == 0)
        return CustomMilesControl::RESUME;
    return CustomMilesControl::NONE;
}

bool CModAudioRuntime::IsControlName(const char* eventName) const
{
    return GetControl(eventName) != CustomMilesControl::NONE;
}

bool CModAudioRuntime::EventNameMatches(const ActiveModAudioEvent& instance, const MilesEventContextPrefix& context)
{
    const char* filterName = context.EventName;
    if (!filterName)
        return false;

    // Native voices match both a compiled bank and a parent event. Direct
    // events have no compiled bank, so match their exact registered name or
    // the event component of Miles' required /bank/event form.
    if (_stricmp(instance.EventName.c_str(), filterName) == 0)
        return true;

    if ((context.FilterFlags & 0x1000) == 0)
    {
        const char* lastSeparator = strrchr(filterName, '/');
        if (lastSeparator && _stricmp(instance.EventName.c_str(), lastSeparator + 1) == 0)
            return true;
    }

    return false;
}

bool CModAudioRuntime::InstanceMatches(const ActiveModAudioEvent& instance, const MilesEventContextPrefix* context)
{
    if (!context)
        return false;

    if ((context->FilterFlags & 1) != 0 && instance.EventId != context->EventId)
        return false;

    if ((context->FilterFlags & 2) != 0 &&
        (instance.ActorPosition[0] != context->FilterActorPosition[0] || instance.ActorPosition[1] != context->FilterActorPosition[1] ||
         instance.ActorPosition[2] != context->FilterActorPosition[2]))
    {
        return false;
    }

    if ((context->FilterFlags & 4) != 0 && !EventNameMatches(instance, *context))
        return false;

    return true;
}

void CModAudioRuntime::ApplyEventControl(const char* controlName, const void* eventContext)
{
    const CustomMilesControl control = GetControl(controlName);
    const auto* context = static_cast<const MilesEventContextPrefix*>(eventContext);
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    auto iter = m_events.begin();
    while (iter != m_events.end())
    {
        if (!InstanceMatches(*iter, context))
        {
            ++iter;
            continue;
        }

        switch (control)
        {
        case CustomMilesControl::STOP_NOW:
            iter = Destroy(iter);
            continue;
        case CustomMilesControl::STOP:
            if (iter->Paused || iter->Definition->FadeOutMs == 0)
            {
                iter = Destroy(iter);
                continue;
            }

            BeginFade(*iter, 0.0f, iter->Definition->FadeOutMs, true);
            if (!iter->Fading)
            {
                iter = Destroy(iter);
                continue;
            }
            break;
        case CustomMilesControl::PAUSE:
            if (!iter->Paused && MilesSamplePause)
            {
                for (ActiveModAudioLayer& layer : iter->Layers)
                    if (layer.Sample)
                        MilesSamplePause(layer.Sample);
                iter->Paused = true;
            }
            break;
        case CustomMilesControl::RESUME:
            if (iter->Paused && MilesSamplePlay)
            {
                for (ActiveModAudioLayer& layer : iter->Layers)
                    if (layer.Sample)
                        MilesSamplePlay(layer.Sample);
                iter->Paused = false;
                iter->LastFadeUpdate = CustomMilesClock::now();
            }
            break;
        case CustomMilesControl::NONE:
        default:
            break;
        }

        ++iter;
    }
}

bool CModAudioRuntime::ConfigureRoute(void* route, const AudioRouteDefinition& definition, const char* eventName)
{
    if (!route)
        return false;

    // Miles stores the active route mode at +0x34: direct=0, panned=1,
    // matrix=2, spatialized=3. The public mode setters silently leave the
    // route unchanged when its input/output channel layouts are incompatible,
    // so verify the requested transition instead of reporting a playable
    // event with a differently configured route.
    constexpr uintptr_t MILES_ROUTE_MODE_OFFSET = 0x34;
    uint8_t expectedMode = 0;

    switch (definition.Mode)
    {
    case AudioRouteMode::DIRECT:
        if (!MilesRouteSetDirect)
            return false;
        MilesRouteSetDirect(route);
        expectedMode = 0;
        break;
    case AudioRouteMode::PANNED:
        if (!MilesRouteSetPanned)
            return false;
        MilesRouteSetPanned(route);
        expectedMode = 1;
        break;
    case AudioRouteMode::SPATIALIZED:
        if (!MilesRouteSetSpatialized)
            return false;
        MilesRouteSetSpatialized(route, nullptr);
        expectedMode = 3;
        break;
    case AudioRouteMode::MIXED:
    {
        if (!MilesRouteSetMixed)
            return false;
        const uintptr_t routeAddress = reinterpret_cast<uintptr_t>(route);
        const size_t matrixSize = static_cast<size_t>(*reinterpret_cast<const uint8_t*>(routeAddress + 0x36)) *
                                  static_cast<size_t>(*reinterpret_cast<const uint8_t*>(routeAddress + 0x37));
        if (matrixSize == 0 || definition.Matrix.size() != matrixSize)
        {
            NS::log::MILES->error("Could not configure custom event {} mixed route: Matrix has {} levels, Miles requires {}", eventName,
                                  definition.Matrix.size(), matrixSize);
            return false;
        }
        MilesRouteSetMixed(route, definition.Matrix.data());
        expectedMode = 2;
        break;
    }
    }

    const uint8_t actualMode = *reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(route) + MILES_ROUTE_MODE_OFFSET);
    if (actualMode != expectedMode)
    {
        NS::log::MILES->error("Could not configure custom event {} route to mode {}: Miles retained mode {} (incompatible channel layout)", eventName,
                              expectedMode, actualMode);
        return false;
    }

    if (!MilesRouteSetVolumeLevel)
        return false;
    MilesRouteSetVolumeLevel(route, definition.Volume);
    if (MilesRouteSetLFELevel)
        MilesRouteSetLFELevel(route, definition.LFEVolume);

    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 1)
        NS::log::MILES->info("Routed custom event {} to Miles bus {}", eventName, definition.Bus);
    return true;
}

void CModAudioRuntime::FinalizeSpatialization(void* sample, MilesSpatializationInfo* spatializationInfo)
{
    NOTE_UNUSED(sample);

    if (spatializationInfo)
        spatializationInfo->Flags = 0;
}

bool CModAudioRuntime::ConfigureSpatialization(void* sample, const ModAudioEventDefinition& definition, const std::array<float, 3>& actorPosition,
                                               const std::array<float, 3>& actorFacing, const std::array<float, 3>& actorUp,
                                               unsigned int listenerMask, const char* eventName)
{
    const bool hasSpatializedRoute = std::ranges::any_of(definition.Layers, [](const AudioLayerDefinition& layer)
    { return std::ranges::any_of(layer.Routes, [](const AudioRouteDefinition& route) { return route.Mode == AudioRouteMode::SPATIALIZED; }); });
    const AudioSpatializationDefinition& spatialization = definition.Spatialization;

    if (hasSpatializedRoute)
    {
        if (!MilesSampleSet3DPosition || !MilesSampleSet3DOrientation || !MilesSampleSet3DUserFalloffFn)
        {
            NS::log::MILES->error("Could not configure custom event {}: Miles 3D APIs are unavailable", eventName);
            return false;
        }

        // MilesInt3DSpatialization initializes the direction, distance, gain,
        // spread, and low-pass fields but intentionally leaves the adjacent
        // routing flags untouched. Compiled events install a post-spatializer
        // callback that finalizes those flags. A direct sample without one can
        // inherit a nonzero stack value, selecting Miles' non-positional
        // default downmix ([1, 1] for mono-to-stereo) instead of its speaker
        // panning matrix. Keep Miles' native calculation and clear only that
        // bypass flag in the supported user-falloff callback stage.
        MilesSampleSet3DUserFalloffFn(sample, FinalizeSpatialization);
        MilesSampleSet3DPosition(sample, actorPosition[0], actorPosition[1], actorPosition[2]);
        MilesSampleSet3DOrientation(sample, actorFacing[0], actorFacing[1], actorFacing[2], actorUp[0], actorUp[1], actorUp[2]);

        if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        {
            NS::log::MILES->info("Custom event {} initial 3D position [{}, {}, {}], listener mask 0x{:08X}", eventName, actorPosition[0],
                                 actorPosition[1], actorPosition[2], spatialization.HasListenerMask ? spatialization.ListenerMask : listenerMask);
        }
    }

    if (MilesSampleSetListenerMask)
        MilesSampleSetListenerMask(sample, spatialization.HasListenerMask ? spatialization.ListenerMask : listenerMask);
    else if (spatialization.HasListenerMask)
    {
        return false;
    }
    if (spatialization.HasAutoSpreadDistance)
    {
        if (!MilesSampleSet3DAutoSpreadDistance)
            return false;
        MilesSampleSet3DAutoSpreadDistance(sample, spatialization.AutoSpreadDistance);
    }
    if (spatialization.HasMultiChannelPan)
    {
        if (!MilesSampleSet3DMultiChannelPan)
            return false;
        MilesSampleSet3DMultiChannelPan(sample, spatialization.MultiChannelPanAngleDegrees, spatialization.MultiChannelPanDistance);
    }
    if (spatialization.HasVolumeCone)
    {
        if (!MilesSampleSet3DVolumeCone)
            return false;
        MilesSampleSet3DVolumeCone(sample, spatialization.VolumeConeEnabled ? 1 : 0, spatialization.VolumeConeInnerAngleDegrees,
                                   spatialization.VolumeConeOuterAngleDegrees, spatialization.VolumeConeOuterVolume);
    }
    if (!spatialization.VolumeCurve.empty())
    {
        if (!MilesSampleSet3DVolumeGraph)
            return false;
        MilesSampleSet3DVolumeGraph(sample, spatialization.VolumeCurve.data(), static_cast<unsigned int>(spatialization.VolumeCurve.size()));
    }
    if (!spatialization.SpreadCurve.empty())
    {
        if (!MilesSampleSet3DSpreadGraph)
            return false;
        MilesSampleSet3DSpreadGraph(sample, spatialization.SpreadCurve.data(), static_cast<unsigned int>(spatialization.SpreadCurve.size()));
    }
    if (!spatialization.LowPassCurve.empty())
    {
        if (!MilesSampleSet3DLowPassGraph)
            return false;
        MilesSampleSet3DLowPassGraph(sample, spatialization.LowPassCurve.data(), static_cast<unsigned int>(spatialization.LowPassCurve.size()));
    }

    return true;
}

bool CModAudioRuntime::ValidateSelectorControllers(void* eventSystem, const std::shared_ptr<AudioSourceSelectorDefinition>& selector,
                                                   const char* eventName)
{
    if (!selector)
        return false;
    if (selector->Mode == AudioSelectorMode::CONTROLLER && selector->ControllerSource == AudioControllerSource::GLOBAL &&
        (!MilesIntControllerIndex || !MilesIntControllerGet ||
         MilesIntControllerIndex(eventSystem, selector->Controller.c_str()) == std::numeric_limits<unsigned int>::max()))
    {
        NS::log::MILES->error("Could not play custom event {}: selector global controller {} does not exist", eventName, selector->Controller);
        return false;
    }
    return std::ranges::all_of(selector->Choices,
                               [eventSystem, eventName](const auto& choice) { return ValidateSelectorControllers(eventSystem, choice, eventName); });
}

AudioPlayResult CModAudioRuntime::TryPlayEvent(void* eventSystem, const char* eventName, uint64_t eventId, const void* eventContext)
{
    std::shared_ptr<ModAudioEventDefinition> definition = m_manager.FindExactDefinition(eventName);
    if (!definition || !definition->IsCustomEvent)
        return CModAudioManager::PlayResult::NOT_CUSTOM;
    Service();

    const auto* context = static_cast<const MilesEventContextPrefix*>(eventContext);
    if (!MilesIntBusLookup || !MilesSampleCreate || !MilesSampleDestroy || !MilesSampleSetSource || !MilesSamplePlay || !MilesSampleCreateRoute ||
        !MilesSampleGetRoute || !MilesSampleSetPlayCount || !MilesSampleSetVolumeLevel || !MilesSampleSetPlaybackRateFactor ||
        !MilesSampleSetLowPassCutoff)
        return CModAudioManager::PlayResult::FAILED;
    if ((!definition->ControllerBindings.empty() ||
         std::ranges::any_of(definition->Layers, [](const AudioLayerDefinition& layer) { return !layer.ControllerBindings.empty(); })) &&
        !MilesEvaluateGraph)
        return CModAudioManager::PlayResult::FAILED;

    // Titanfall 2 MilesEventSystem::driver is +0x10. MilesIntBusLookup returns a
    // MilesRuntimeBusV13 whose live MilesBus output is +0x08.
    void* driver = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(eventSystem) + 0x10);
    if (!driver)
    {
        NS::log::MILES->error("Could not play custom event {}: Miles event driver is not active", eventName);
        return CModAudioManager::PlayResult::FAILED;
    }

    auto validateBindings = [&](const std::vector<AudioControllerBinding>& bindings)
    {
        for (const AudioControllerBinding& binding : bindings)
        {
            if (binding.Source == AudioControllerSource::GLOBAL &&
                (!MilesIntControllerIndex || !MilesIntControllerGet ||
                 MilesIntControllerIndex(eventSystem, binding.Controller.c_str()) == std::numeric_limits<unsigned int>::max()))
            {
                NS::log::MILES->error("Could not play custom event {}: global Miles controller {} does not exist", eventName, binding.Controller);
                return false;
            }
        }
        return true;
    };
    if (!validateBindings(definition->ControllerBindings))
        return CModAudioManager::PlayResult::FAILED;
    for (const AudioLayerDefinition& layer : definition->Layers)
    {
        if (!validateBindings(layer.ControllerBindings) || !ValidateSelectorControllers(eventSystem, layer.Selector, eventName))
            return CModAudioManager::PlayResult::FAILED;
    }

    std::array<float, 3> actorPosition{};
    std::array<float, 3> actorFacing = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> actorUp = {0.0f, 0.0f, 1.0f};
    if (context && context->ActorPosition)
        memcpy(actorPosition.data(), context->ActorPosition, sizeof(actorPosition));
    if (context && context->ActorFacing)
        memcpy(actorFacing.data(), context->ActorFacing, sizeof(actorFacing));
    if (context && context->ActorUp)
        memcpy(actorUp.data(), context->ActorUp, sizeof(actorUp));
    if (definition->Spatialization.HasOrientation)
    {
        actorFacing = definition->Spatialization.Facing;
        actorUp = definition->Spatialization.Up;
    }
    const unsigned int listenerMask = context ? context->ListenerMask : 1u;

    ActiveModAudioEvent instance{
        .EventSystem = eventSystem,
        .Definition = definition,
        .EventName = eventName,
        .EventId = eventId,
        .ActorPosition = actorPosition,
        .ActorFacing = actorFacing,
        .ActorUp = actorUp,
        .LastPositionUpdate = CustomMilesClock::now(),
        .EventRateFactor = context ? std::max(context->RateFactor, 0.01f) : 1.0f,
        .FadeMultiplier = definition->FadeInMs == 0 ? 1.0f : 0.0f,
        .FadeTargetMultiplier = definition->FadeInMs == 0 ? 1.0f : 0.0f,
    };

    const MilesQueuedControllerValue* controller = context ? static_cast<const MilesQueuedControllerValue*>(context->ControllerValues) : nullptr;
    for (unsigned int controllerCount = 0; controller && controllerCount < 64; ++controllerCount, controller = controller->Previous)
    {
        if (controller->Opcode != 0xF || controller->Size < 25 || controller->Size > 1000)
            continue;
        instance.EventControllerValues[controller->Name] = controller->Value;
    }

    auto destroyUntrackedLayers = [&]()
    {
        for (ActiveModAudioLayer& layer : instance.Layers)
        {
            if (layer.Sample)
                MilesSampleDestroy(layer.Sample);
            layer.Sample = nullptr;
        }
    };
    std::uniform_real_distribution<float> randomUnit(-1.0f, 1.0f);

    for (const AudioLayerDefinition& layerDefinition : definition->Layers)
    {
        std::vector<ResolvedAudioRoute> routes;
        for (const AudioRouteDefinition& route : layerDefinition.Routes)
        {
            void* runtimeBusRecord = MilesIntBusLookup(eventSystem, route.Bus.c_str());
            void* runtimeBus = runtimeBusRecord ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(runtimeBusRecord) + 0x08) : nullptr;
            if (!runtimeBus)
            {
                NS::log::MILES->error("Could not play custom event {} layer {}: Miles bus {} is unavailable", eventName, layerDefinition.Name,
                                      route.Bus);
                destroyUntrackedLayers();
                return CModAudioManager::PlayResult::FAILED;
            }
            routes.push_back({.Definition = &route, .RuntimeBus = runtimeBus});
        }

        const AudioSampleData* source = SelectLayerAudioSource(*definition, layerDefinition, eventSystem, instance.EventControllerValues);
        if (!source || (layerDefinition.SourceMode == AudioSourceMode::MEMORY && !source->Data))
        {
            NS::log::MILES->error("Could not select source for custom event {} layer {}", eventName, layerDefinition.Name);
            destroyUntrackedLayers();
            return CModAudioManager::PlayResult::FAILED;
        }

        void* primaryRouteInitialization = routes.front().Definition->Mode == AudioRouteMode::SPATIALIZED ? reinterpret_cast<void*>(1) : nullptr;
        ActiveModAudioLayer layer;
        layer.Sample = MilesSampleCreate(driver, routes.front().RuntimeBus, primaryRouteInitialization);
        layer.Definition = &layerDefinition;
        if (!layer.Sample)
        {
            destroyUntrackedLayers();
            return CModAudioManager::PlayResult::FAILED;
        }
        instance.Layers.push_back(std::move(layer));
        ActiveModAudioLayer& activeLayer = instance.Layers.back();

        s_loadingCustomMilesSample = true;
        bool sourceLoaded = false;
        if (layerDefinition.SourceMode == AudioSourceMode::STREAM)
        {
            sourceLoaded = MilesSampleSetSourceStream && MilesSampleSetSourceStream(activeLayer.Sample, source->Path.string().c_str(),
                                                                                    layerDefinition.StreamBufferBytes, nullptr, 0);
        }
        else
        {
            sourceLoaded = MilesSampleSetSource(activeLayer.Sample, source->Data.get(), static_cast<unsigned int>(source->Size), 64);
        }
        s_loadingCustomMilesSample = false;
        if (!sourceLoaded)
        {
            NS::log::MILES->error("Could not load source {} for custom event {} layer {}", source->Path.string(), eventName, layerDefinition.Name);
            destroyUntrackedLayers();
            return CModAudioManager::PlayResult::FAILED;
        }

        void* primaryRoute = MilesSampleGetRoute(activeLayer.Sample, 0);
        if (!ConfigureRoute(primaryRoute, *routes.front().Definition, eventName))
        {
            destroyUntrackedLayers();
            return CModAudioManager::PlayResult::FAILED;
        }
        activeLayer.Routes.push_back(primaryRoute);
        for (size_t routeIndex = 1; routeIndex < routes.size(); ++routeIndex)
        {
            void* routeInitialization = routes[routeIndex].Definition->Mode == AudioRouteMode::SPATIALIZED ? reinterpret_cast<void*>(1) : nullptr;
            void* route =
                MilesSampleCreateRoute(activeLayer.Sample, routes[routeIndex].RuntimeBus, routes[routeIndex].Definition->Volume, routeInitialization);
            if (!route || !ConfigureRoute(route, *routes[routeIndex].Definition, eventName))
            {
                destroyUntrackedLayers();
                return CModAudioManager::PlayResult::FAILED;
            }
            activeLayer.Routes.push_back(route);
        }

        MilesSampleSetPlayCount(activeLayer.Sample, layerDefinition.PlayCount);
        if (layerDefinition.LoopStartSamples != 0 || layerDefinition.LoopEndSamples != -1)
        {
            if (!MilesSampleGetDurationSamples || !MilesSampleSetLoopSamples)
            {
                destroyUntrackedLayers();
                return CModAudioManager::PlayResult::FAILED;
            }
            const unsigned int durationSamples = MilesSampleGetDurationSamples(activeLayer.Sample);
            const unsigned int loopEnd =
                layerDefinition.LoopEndSamples == -1 ? durationSamples : static_cast<unsigned int>(layerDefinition.LoopEndSamples);
            if (durationSamples == 0 || loopEnd > durationSamples || static_cast<unsigned int>(layerDefinition.LoopStartSamples) >= loopEnd)
            {
                NS::log::MILES->error("Invalid loop range for custom event {} layer {}", eventName, layerDefinition.Name);
                destroyUntrackedLayers();
                return CModAudioManager::PlayResult::FAILED;
            }
            MilesSampleSetLoopSamples(activeLayer.Sample, layerDefinition.LoopStartSamples, static_cast<int>(loopEnd));
        }
        if (layerDefinition.StartPositionSamples != 0)
        {
            if (!MilesSampleSetPositionSamples)
            {
                destroyUntrackedLayers();
                return CModAudioManager::PlayResult::FAILED;
            }
            MilesSampleSetPositionSamples(activeLayer.Sample, layerDefinition.StartPositionSamples);
        }
        if (!ConfigureSpatialization(activeLayer.Sample, *definition, actorPosition, actorFacing, actorUp, listenerMask, eventName))
        {
            destroyUntrackedLayers();
            return CModAudioManager::PlayResult::FAILED;
        }
        if (layerDefinition.Panning.HasLevels)
        {
            if (!MilesSampleSetPanLevels)
            {
                destroyUntrackedLayers();
                return CModAudioManager::PlayResult::FAILED;
            }
            MilesSampleSetPanLevels(activeLayer.Sample, layerDefinition.Panning.Levels[0], layerDefinition.Panning.Levels[1],
                                    layerDefinition.Panning.Levels[2], layerDefinition.Panning.Levels[3], layerDefinition.Panning.Levels[4]);
        }
        if (layerDefinition.DopplerFactor > 0.0f && MilesSampleSetDopplerFactor)
        {
            const std::array<float, 3> zeroVelocity{};
            MilesSampleSetDopplerFactor(activeLayer.Sample, layerDefinition.DopplerFactor, nullptr, zeroVelocity.data(),
                                        layerDefinition.MetersPerGameUnit);
        }
        for (const AudioFilterDefinition& filter : layerDefinition.Filters)
        {
            void* runtimeFilter = MilesSampleAddFilterByName ? MilesSampleAddFilterByName(activeLayer.Sample, filter.Name.c_str()) : nullptr;
            if (!runtimeFilter)
            {
                NS::log::MILES->error("Could not add Miles filter {} to custom event {} layer {}", filter.Name, eventName, layerDefinition.Name);
                destroyUntrackedLayers();
                return CModAudioManager::PlayResult::FAILED;
            }
            activeLayer.Filters.push_back(runtimeFilter);
        }

        activeLayer.RandomVolumeFactor = std::pow(10.0f, randomUnit(RandomGenerator()) * layerDefinition.VolumeRandomDb / 20.0f);
        const float randomizedPitch = layerDefinition.PitchSemitones + randomUnit(RandomGenerator()) * layerDefinition.PitchRandomSemitones;
        activeLayer.RandomPitchFactor = std::pow(2.0f, randomizedPitch / 12.0f);
        activeLayer.AppliedRouteVolumes.assign(activeLayer.Routes.size(), -1.0f);
        activeLayer.AppliedRouteLFEVolumes.assign(activeLayer.Routes.size(), -1.0f);
        activeLayer.AppliedRouteMatrices.resize(activeLayer.Routes.size());
        activeLayer.AppliedFilterWet.assign(activeLayer.Filters.size(), -1.0f);
        activeLayer.AppliedFilterDry.assign(activeLayer.Filters.size(), -1.0f);
        activeLayer.AppliedFilterProperties.resize(activeLayer.Filters.size());
    }

    ApplyProperties(instance);
    if (definition->FadeInMs != 0)
        BeginFade(instance, 1.0f, definition->FadeInMs, false);

    const bool useScheduledPlayback = instance.Layers.size() > 1 || std::ranges::any_of(definition->Layers, [](const AudioLayerDefinition& layer)
    { return layer.StartDelayMs != 0; });
    unsigned __int64 scheduleBase = 0;
    unsigned int driverSampleRate = 0;
    if (useScheduledPlayback)
    {
        if (!MilesSamplePlayScheduled || !MilesDriverGetMixedSamples || !MilesDriverGetSampleRate)
        {
            destroyUntrackedLayers();
            return CModAudioManager::PlayResult::FAILED;
        }
        driverSampleRate = MilesDriverGetSampleRate(driver);
        scheduleBase = MilesDriverGetMixedSamples(driver) + driverSampleRate / 100;
    }

    std::vector<std::pair<void*, unsigned int>> samplesToPlay;
    for (size_t index = 0; index < instance.Layers.size(); ++index)
        samplesToPlay.push_back({instance.Layers[index].Sample, definition->Layers[index].StartDelayMs});

    {
        std::unique_lock lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock())
        {
            destroyUntrackedLayers();
            NS::log::MILES->warn("Could not track custom event {} while audio overrides are being reloaded", eventName);
            return CModAudioManager::PlayResult::FAILED;
        }

        if (definition->MaxInstances != 0)
        {
            unsigned int instanceCount = static_cast<unsigned int>(std::count_if(
                m_events.begin(), m_events.end(), [eventName](const ActiveModAudioEvent& active) { return active.EventName == eventName; }));

            if (instanceCount >= definition->MaxInstances && definition->InstanceLimitPolicy == AudioInstanceLimitPolicy::REJECT_NEW)
            {
                destroyUntrackedLayers();
                if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
                    NS::log::MILES->info("Rejected custom event {} at its instance limit", eventName);
                return CModAudioManager::PlayResult::HANDLED;
            }

            auto iter = m_events.begin();
            while (definition->InstanceLimitPolicy == AudioInstanceLimitPolicy::STEAL_OLDEST && instanceCount >= definition->MaxInstances &&
                   iter != m_events.end())
            {
                if (iter->EventName != eventName)
                {
                    ++iter;
                    continue;
                }

                iter = Destroy(iter);
                --instanceCount;
            }
        }

        m_events.push_back(std::move(instance));
    }

    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        NS::log::MILES->info("Playing custom event {} with {} layer(s)", eventName, samplesToPlay.size());

    for (const auto& [sample, delayMs] : samplesToPlay)
    {
        if (useScheduledPlayback)
            MilesSamplePlayScheduled(sample, scheduleBase + static_cast<unsigned __int64>(driverSampleRate) * delayMs / 1000);
        else
            MilesSamplePlay(sample);
    }
    return CModAudioManager::PlayResult::HANDLED;
}

DECLARE_HOOK(ServerSoundLookupForEmit, server.dll + 0x641750,
             [](auto& hook, char* eventName, uint64_t* eventHash, float* durationSeconds, uint8_t* reliable) -> char*
{
    g_ModAudioManager.ClearActiveServerSoundAlias();

    char* nativeDefinition = hook.Original(eventName, eventHash, durationSeconds, reliable);
    if (nativeDefinition)
        return nativeDefinition;

    ServerSoundAliasDefinition* definition = g_ModAudioManager.ActivateServerSoundAlias(eventName);
    if (!definition)
        return nullptr;

    if (eventHash)
        *eventHash = definition->EventHash;
    if (durationSeconds)
        *durationSeconds = definition->DurationSeconds;
    if (reliable)
        *reliable = definition->DurationSeconds <= 0.0f || definition->DurationSeconds > 15.0f;

    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        NS::log::MILES->info("Resolved custom server event {}", definition->EventName);

    return reinterpret_cast<char*>(definition);
})

DECLARE_HOOK(ServerSoundLookupForStop, server.dll + 0x641870, [](auto& hook, char* eventName, uint64_t* eventHash, uint8_t* reliable) -> char*
{
    g_ModAudioManager.ClearActiveServerSoundAlias();

    char* nativeDefinition = hook.Original(eventName, eventHash, reliable);
    if (nativeDefinition)
        return nativeDefinition;

    ServerSoundAliasDefinition* definition = g_ModAudioManager.ActivateServerSoundAlias(eventName);
    if (!definition)
        return nullptr;

    if (eventHash)
        *eventHash = definition->EventHash;
    if (reliable)
        *reliable = definition->DurationSeconds <= 0.0f || definition->DurationSeconds > 15.0f;
    return reinterpret_cast<char*>(definition);
})

DECLARE_HOOK(ServerSoundAliasExists, server.dll + 0x642090, [](auto& hook, char* eventName) -> bool
{
    if (hook.Original(eventName))
        return true;
    return g_ModAudioManager.HasServerSoundAlias(eventName);
})

DECLARE_HOOK(ServerSoundGetTags, server.dll + 0x643460, [](auto& hook, const char* eventName) -> uint32_t
{
    uint32_t soundTags = 0;
    if (g_ModAudioManager.TryGetServerSoundTags(eventName, soundTags))
        return soundTags;
    return hook.Original(eventName);
})

// The client normally refuses unknown sound names before they ever reach
// Miles. Supply the minimum 48-byte definition used by its shared sound
// dispatcher so UI, entity, position, and control helpers retain their normal
// queue context and event-handle bookkeeping.
DECLARE_HOOK(ClientSoundLookupByName, client.dll + 0x581210, [](auto& hook, void** outDefinition, const char* eventName) -> bool
{
    ClientSoundEventDefinition* definition = g_ModAudioManager.ActivateClientSoundEvent(eventName);
    if (!definition)
        return hook.Original(outDefinition, eventName);

    if (outDefinition)
        *outDefinition = definition->GetClientDefinition();
    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        NS::log::MILES->info("Resolved custom event {} by name", definition->GetEventName());
    return true;
})

DECLARE_HOOK(ClientSoundDispatch, client.dll + 0x5796D0, [](auto& hook, void* request) -> signed __int64
{
    CModAudioManager::CActiveClientSoundDispatch activeDispatch(g_ModAudioManager);
    return hook.Original(request);
})

// Native suffix selection performs a second hash-table lookup. Custom events
// deliberately keep their base definition because their behavior is supplied
// by the mod definition rather than compiled client sound metadata.
DECLARE_HOOK(ClientSoundLookupByHash, client.dll + 0x5811A0, [](auto& hook, void** outDefinition, uint64_t hash) -> bool
{
    ClientSoundEventDefinition* definition = g_ModAudioManager.GetActiveClientSoundEvent();
    if (g_ModAudioManager.HasActiveClientSoundDispatch() && definition)
    {
        if (outDefinition)
            *outDefinition = definition->GetClientDefinition();
        return true;
    }

    // Replicated/entity sounds can enter the client through one of the
    // hash-based play helpers, never performing the name lookup above. Keep
    // the matching custom definition alive through the shared dispatcher so
    // its final Miles template-ID command can be replaced by our named event.
    definition = g_ModAudioManager.ActivateClientSoundEvent(hash);
    if (!definition)
        return hook.Original(outDefinition, hash);

    if (outDefinition)
        *outDefinition = definition->GetClientDefinition();
    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        NS::log::MILES->info("Resolved custom event {} by hash", definition->GetEventName());
    return true;
})

// Preserve every option the client staged on the Miles queue, replacing only
// the final compiled-event command with the named-event command. Command 4 is
// serviced by MilesIntRunEvent, where custom events are resolved below.
DECLARE_HOOK(MilesQueueEventRunByDefinitionId, mileswin64.dll + 0x33AD0,
             [](auto& hook, void* queue, const int* compiledEventDefinitionId) -> signed __int64
{
    ClientSoundEventDefinition* activeDefinition = g_ModAudioManager.GetActiveClientSoundEvent();
    if (!activeDefinition || compiledEventDefinitionId != activeDefinition->GetMilesEventDefinitionId())
        return hook.Original(queue, compiledEventDefinitionId);

    std::shared_ptr<ClientSoundEventDefinition> definition = g_ModAudioManager.TakeActiveClientSoundEvent();
    if (!MilesQueueEventRun)
    {
        NS::log::MILES->error("Could not queue custom event {}: Miles named-event API is unavailable", definition->GetEventName());
        return 0;
    }

    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        NS::log::MILES->info("Queuing custom event {} through Miles", definition->GetEventName());

    return MilesQueueEventRun(queue, definition->GetEventName().c_str());
})

// Custom events intentionally bypass compiled event actions while retaining
// Miles' event-instance controls. Routes, spatialization, controllers, loop
// behavior, and lifecycle policy all come from the mod definition.
DECLARE_HOOK(MilesIntRunEvent, mileswin64.dll + 0x31B50,
             [](auto& hook, void* eventSystem, const char* eventName, unsigned __int64 eventId, void* context, int recursionDepth) -> unsigned int
{
    if (g_ModAudioManager.IsCustomEventControl(eventName))
    {
        g_ModAudioManager.ApplyCustomEventControl(eventName, context);
        return hook.Original(eventSystem, eventName, eventId, context, recursionDepth);
    }

    const CModAudioManager::PlayResult result = g_ModAudioManager.TryPlayCustomEvent(eventSystem, eventName, eventId, context);
    if (result == CModAudioManager::PlayResult::NOT_CUSTOM)
        return hook.Original(eventSystem, eventName, eventId, context, recursionDepth);
    return result == CModAudioManager::PlayResult::HANDLED ? 1u : 0u;
})

void CModAudioRuntime::UpdateEventPosition(uint64_t eventId, float x, float y, float z)
{
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    for (ActiveModAudioEvent& event : m_events)
    {
        if (event.EventId != eventId)
            continue;

        const CustomMilesClock::time_point now = CustomMilesClock::now();
        const float elapsedSeconds = std::chrono::duration<float>(now - event.LastPositionUpdate).count();
        const std::array<float, 3> newPosition = {x, y, z};
        std::array<float, 3> sampleVelocity{};
        if (elapsedSeconds > 0.0001f)
        {
            for (size_t axis = 0; axis < sampleVelocity.size(); ++axis)
                sampleVelocity[axis] = (newPosition[axis] - event.ActorPosition[axis]) / elapsedSeconds;
            event.SampleVelocity = sampleVelocity;
        }

        event.ActorPosition = newPosition;
        event.LastPositionUpdate = now;
        for (ActiveModAudioLayer& layer : event.Layers)
        {
            if (!layer.Sample)
                continue;
            if (MilesSampleSet3DPosition)
                MilesSampleSet3DPosition(layer.Sample, x, y, z);
            if (elapsedSeconds > 0.0001f && MilesSampleSetDopplerFactor && layer.Definition && layer.Definition->DopplerFactor > 0.0f)
            {
                MilesSampleSetDopplerFactor(layer.Sample, layer.Definition->DopplerFactor, event.ListenerVelocity.data(), sampleVelocity.data(),
                                            layer.Definition->MetersPerGameUnit);
            }
        }
    }
}

void CModAudioRuntime::UpdateEventOrientation(uint64_t eventId, float facingX, float facingY, float facingZ, float upX, float upY, float upZ)
{
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    for (ActiveModAudioEvent& event : m_events)
    {
        if (event.EventId != eventId || event.Definition->Spatialization.HasOrientation)
            continue;

        event.ActorFacing = {facingX, facingY, facingZ};
        event.ActorUp = {upX, upY, upZ};
        if (MilesSampleSet3DOrientation)
        {
            for (ActiveModAudioLayer& layer : event.Layers)
                if (layer.Sample)
                    MilesSampleSet3DOrientation(layer.Sample, facingX, facingY, facingZ, upX, upY, upZ);
        }
    }
}

void CModAudioRuntime::RecordSpatialization(void* sample, void* route, const void* listener, const float* outputLevels, uint64_t outputChannelCount,
                                            uint64_t inputChannelCount)
{
    if (!sample || !route || !listener || !outputLevels)
        return;
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    ActiveModAudioEvent* event = nullptr;
    ActiveModAudioLayer* activeLayer = nullptr;
    for (ActiveModAudioEvent& candidate : m_events)
    {
        auto layer = std::find_if(candidate.Layers.begin(), candidate.Layers.end(),
                                  [sample](const ActiveModAudioLayer& active) { return active.Sample == sample; });
        if (layer != candidate.Layers.end())
        {
            event = &candidate;
            activeLayer = &*layer;
            break;
        }
    }
    if (!event || !activeLayer)
        return;

    const uintptr_t routeAddress = reinterpret_cast<uintptr_t>(route);
    const uintptr_t listenerAddress = reinterpret_cast<uintptr_t>(listener);
    const uint8_t routeMode = *reinterpret_cast<const uint8_t*>(routeAddress + 0x34);
    const uint8_t routeOutputChannels = *reinterpret_cast<const uint8_t*>(routeAddress + 0x37);
    const float* listenerPosition = reinterpret_cast<const float*>(listenerAddress + 0x04);

    const CustomMilesClock::time_point now = CustomMilesClock::now();
    const float listenerElapsed = std::chrono::duration<float>(now - event->LastListenerUpdate).count();
    if (event->HasListenerPosition && listenerElapsed > 0.0001f)
    {
        for (size_t axis = 0; axis < event->ListenerVelocity.size(); ++axis)
            event->ListenerVelocity[axis] = (listenerPosition[axis] - event->LastListenerPosition[axis]) / listenerElapsed;
    }
    std::copy_n(listenerPosition, event->LastListenerPosition.size(), event->LastListenerPosition.begin());
    event->LastListenerUpdate = now;
    event->HasListenerPosition = true;

    if (!Cvar_ns_print_played_sounds || Cvar_ns_print_played_sounds->GetInt() < 2 || activeLayer->LoggedSpatialization)
        return;
    activeLayer->LoggedSpatialization = true;

    const float left = outputChannelCount > 0 && inputChannelCount > 0 ? outputLevels[0] : 0.0f;
    const float right = outputChannelCount > 1 && inputChannelCount > 0 ? outputLevels[1] : 0.0f;
    NS::log::MILES->info("Custom event {} native 3D matrix: route mode {}, input channels {}, output channels {} (route {}), "
                         "listener [{}, {}, {}], source [{}, {}, {}], first-channel L/R [{}, {}]",
                         event->EventName, routeMode, inputChannelCount, outputChannelCount, routeOutputChannels, listenerPosition[0],
                         listenerPosition[1], listenerPosition[2], event->ActorPosition[0], event->ActorPosition[1], event->ActorPosition[2], left,
                         right);
}

void CModAudioRuntime::UpdateEventController(uint64_t eventId, const char* controllerName, float value)
{
    if (!controllerName)
        return;
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    for (ActiveModAudioEvent& event : m_events)
    {
        if (event.EventId != eventId)
            continue;
        event.EventControllerValues[controllerName] = value;
        ApplyProperties(event);
    }
}

void CModAudioRuntime::UpdateEventRate(uint64_t eventId, float rateFactor)
{
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    for (ActiveModAudioEvent& event : m_events)
    {
        if (event.EventId != eventId)
            continue;
        event.EventRateFactor = std::max(rateFactor, 0.01f);
        ApplyProperties(event);
    }
}

DECLARE_HOOK(MilesQueueActiveEvent3DPosition, mileswin64.dll + 0x33F20,
             [](auto& hook, void* queue, unsigned __int64 eventId, float x, float y, float z) -> unsigned int
{
    const unsigned int result = hook.Original(queue, eventId, x, y, z);
    g_ModAudioManager.UpdateCustomEventPosition(eventId, x, y, z);
    return result;
})

DECLARE_HOOK(MilesQueueActiveEvent3DOrientation, mileswin64.dll + 0x33F80,
             [](auto& hook, void* queue, unsigned __int64 eventId, float facingX, float facingY, float facingZ, float upX, float upY,
                float upZ) -> unsigned int
{
    const unsigned int result = hook.Original(queue, eventId, facingX, facingY, facingZ, upX, upY, upZ);
    g_ModAudioManager.UpdateCustomEventOrientation(eventId, facingX, facingY, facingZ, upX, upY, upZ);
    return result;
})

// Route mode alone does not prove that Miles is producing a directional
// matrix. Log the first matrix emitted by the native low-level spatializer for
// each custom voice so route, listener, and speaker-layout regressions are
// visible at ns_print_played_sounds 2.
DECLARE_HOOK(MilesStaLowLevelSpatializeDebug, mileswin64.dll + 0x125F0,
             [](auto& hook, void* sample, void* route, const void* listener, float* outputLevels, unsigned __int64 outputChannelCount,
                unsigned __int64 inputChannelCount) -> float
{
    const float lowPassCutoff = hook.Original(sample, route, listener, outputLevels, outputChannelCount, inputChannelCount);
    g_ModAudioManager.RecordSpatialization(sample, route, listener, outputLevels, outputChannelCount, inputChannelCount);
    return lowPassCutoff;
})

DECLARE_HOOK(MilesQueueActiveEventControllerValue, mileswin64.dll + 0x34020,
             [](auto& hook, void* queue, unsigned __int64 eventId, const char* controllerName, float value) -> unsigned int
{
    const unsigned int result = hook.Original(queue, eventId, controllerName, value);
    g_ModAudioManager.UpdateCustomEventController(eventId, controllerName, value);
    return result;
})

DECLARE_HOOK(MilesQueueActiveEventRateFactor, mileswin64.dll + 0x342B0,
             [](auto& hook, void* queue, unsigned __int64 eventId, float rateFactor) -> bool
{
    const bool result = hook.Original(queue, eventId, rateFactor);
    g_ModAudioManager.UpdateCustomEventRate(eventId, rateFactor);
    return result;
})

DECLARE_HOOK(MilesIntServiceProjectEventSystem, mileswin64.dll + 0x1B2D0, [](auto& hook, void* eventSystem) -> int
{
    const int result = hook.Original(eventSystem);
    g_ModAudioManager.ServiceActiveEvents();
    return result;
})

DECLARE_HOOK(LoadSampleMetadata, mileswin64.dll + 0xF110,
             [](auto& hook, void* sample, void* audioBuffer, unsigned int audioBufferLength, int audioType) -> bool
{
    if (s_loadingCustomMilesSample)
        return hook.Original(sample, audioBuffer, audioBufferLength, audioType);

    if (audioType == 0)
        return hook.Original(sample, audioBuffer, audioBufferLength, audioType);

    const char* eventName = pszAudioEventName;
    if (!eventName)
        return hook.Original(sample, audioBuffer, audioBufferLength, audioType);

    if (Cvar_ns_print_played_sounds && Cvar_ns_print_played_sounds->GetInt() > 0)
        NS::log::MILES->info("Playing event {}", eventName);

    void* data = nullptr;
    unsigned int dataLength = 0;
    if (!g_ModAudioManager.TryGetReplacementSample(eventName, data, dataLength))
        return hook.Original(sample, audioBuffer, audioBufferLength, audioType);

    audioBuffer = data;
    audioBufferLength = dataLength;
    *(void**)((uintptr_t)sample + 0xE8) = audioBuffer;
    *(unsigned int*)((uintptr_t)sample + 0xF0) = audioBufferLength;

    bool res = hook.Original(sample, audioBuffer, audioBufferLength, 64);
    if (!res)
        NS::log::MILES->error("LoadSampleMetadata failed! The game will crash :(");

    return res;
})
DECLARE_HOOK(Sub_1800294C0, mileswin64.dll + 0x294C0, [](auto& hook, void* a1, void* a2)
{
    NOTE_UNUSED(hook);
    const char* previousEventName = pszAudioEventName;
    pszAudioEventName = reinterpret_cast<const char*>((*((__int64*)a2 + 6)));
    hook.Original(a1, a2);
    pszAudioEventName = previousEventName;
})
DECLARE_HOOK(Sub_18003EBD0, mileswin64.dll + 0x3EBD0, [](auto& hook, DWORD dwThreadID, const char* threadName)
{
    NOTE_UNUSED(hook);
    if (!threadName)
    {
        hook.Original(dwThreadID, threadName);
        return;
    }

    HANDLE hThread = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, dwThreadID);

    if (hThread != NULL)
    {
        auto tmp = std::string(threadName);
        if (pSetThreadDescription)
            pSetThreadDescription(hThread, std::wstring(tmp.begin(), tmp.end()).c_str());

        CloseHandle(hThread);
    }

    hook.Original(dwThreadID, threadName);
})
DECLARE_HOOK(Sub_18003BC10, mileswin64.dll + 0x3BC10, [](auto& hook, void* a1, void* a2, void* a3, void* a4, void* a5, int a6) -> char*
{
    NOTE_UNUSED(hook);
    HANDLE hThread;
    char* ret = hook.Original(a1, a2, a3, a4, a5, a6);

    if (ret != NULL && (hThread = reinterpret_cast<HANDLE>(*((uint64_t*)ret + 55))) != NULL)
    {
        if (pSetThreadDescription)
            pSetThreadDescription(hThread, L"[Miles] WASAPI Service Thread");
    }

    return ret;
})
DECLARE_HOOK(MilesLog, client.dll + 0x57DAD0, [](auto& hook, int level, const char* string)
{
    NOTE_UNUSED(hook);
    if (!Cvar_mileslog_enable || !Cvar_mileslog_enable->GetBool())
        return;

    NS::log::MILES->info("{} - {}", level, string);
})

ON_DLL_LOAD("mileswin64.dll", MilesWin64_Audio, [](CModule module)
{
    MilesQueueEventRun = module.Offset(0x339D0).RCast<MilesQueueEventRun_Type>();
    MilesEvaluateGraph = module.Offset(0x2290).RCast<MilesEvaluateGraph_Type>();
    MilesIntBusLookup = module.Offset(0x26560).RCast<MilesIntBusLookup_Type>();
    MilesIntControllerIndex = module.Offset(0x27B90).RCast<MilesIntControllerIndex_Type>();
    MilesIntControllerGet = module.Offset(0x27C00).RCast<MilesIntControllerGet_Type>();
    MilesSampleCreate = module.Offset(0x4EA0).RCast<MilesSampleCreate_Type>();
    MilesSampleDestroy = module.Offset(0x4F50).RCast<MilesSampleDestroy_Type>();
    MilesSampleGetStatus = module.Offset(0x4FD0).RCast<MilesSampleGetStatus_Type>();
    MilesSampleSetSource = module.Offset(0x5070).RCast<MilesSampleSetSource_Type>();
    MilesSampleSetSourceStream = module.Offset(0x52D0).RCast<MilesSampleSetSourceStream_Type>();
    MilesSampleCreateRoute = module.Offset(0x5860).RCast<MilesSampleCreateRoute_Type>();
    MilesSampleGetRoute = module.Offset(0x5920).RCast<MilesSampleGetRoute_Type>();
    MilesSamplePlay = module.Offset(0x6190).RCast<MilesSamplePlay_Type>();
    MilesSamplePlayScheduled = module.Offset(0x6470).RCast<MilesSamplePlayScheduled_Type>();
    MilesSamplePause = module.Offset(0x6210).RCast<MilesSamplePause_Type>();
    MilesSampleSetPanLevels = module.Offset(0x5D60).RCast<MilesSampleSetPanLevels_Type>();
    MilesSampleSetPanLeftRight = module.Offset(0x5E50).RCast<MilesSampleSetPan_Type>();
    MilesSampleSetPanFrontBack = module.Offset(0x5EF0).RCast<MilesSampleSetPan_Type>();
    MilesSampleSetPan360 = module.Offset(0x5F90).RCast<MilesSampleSetPan_Type>();
    MilesSampleSetLFEVolumeLevel = module.Offset(0x5C00).RCast<MilesSampleSetLFEVolumeLevel_Type>();
    MilesSampleSetPlaybackRateFactor = module.Offset(0x6940).RCast<MilesSampleSetPlaybackRateFactor_Type>();
    MilesSampleSetLowPassCutoff = module.Offset(0x6B50).RCast<MilesSampleSetLowPassCutoff_Type>();
    MilesSampleSetDopplerFactor = module.Offset(0x6BF0).RCast<MilesSampleSetDopplerFactor_Type>();
    MilesSampleSetListenerMask = module.Offset(0x6CB0).RCast<MilesSampleSetListenerMask_Type>();
    MilesSampleSet3DPosition = module.Offset(0x6D80).RCast<MilesSampleSet3DPosition_Type>();
    MilesSampleSet3DOrientation = module.Offset(0x6E50).RCast<MilesSampleSet3DOrientation_Type>();
    MilesSampleSet3DAutoSpreadDistance = module.Offset(0x6F30).RCast<MilesSampleSet3DAutoSpreadDistance_Type>();
    MilesSampleSet3DMultiChannelPan = module.Offset(0x6FE0).RCast<MilesSampleSet3DMultiChannelPan_Type>();
    MilesSampleSet3DSpreadGraph = module.Offset(0x70D0).RCast<MilesSampleSet3DGraph_Type>();
    MilesSampleSet3DLowPassGraph = module.Offset(0x7180).RCast<MilesSampleSet3DGraph_Type>();
    MilesSampleSet3DVolumeGraph = module.Offset(0x7230).RCast<MilesSampleSet3DGraph_Type>();
    MilesSampleSet3DVolumeCone = module.Offset(0x72E0).RCast<MilesSampleSet3DVolumeCone_Type>();
    MilesSampleSet3DUserFalloffFn = module.Offset(0x73E0).RCast<MilesSampleSet3DUserFalloffFn_Type>();
    MilesSampleSetPlayCount = module.Offset(0x7490).RCast<MilesSampleSetPlayCount_Type>();
    MilesSampleSetLoopSamples = module.Offset(0x7540).RCast<MilesSampleSetLoopSamples_Type>();
    MilesSampleSetPositionSamples = module.Offset(0x7730).RCast<MilesSampleSetPositionSamples_Type>();
    MilesSampleGetDurationSamples = module.Offset(0x5590).RCast<MilesSampleGetDurationSamples_Type>();
    MilesSampleSetVolumeLevel = module.Offset(0x5AA0).RCast<MilesSampleSetVolumeLevel_Type>();
    MilesRouteSetVolumeLevel = module.Offset(0x4530).RCast<MilesRouteSetVolumeLevel_Type>();
    MilesRouteSetLFELevel = module.Offset(0x4670).RCast<MilesRouteSetLFELevel_Type>();
    MilesRouteSetPanned = module.Offset(0x4850).RCast<MilesRouteSetMode_Type>();
    MilesRouteSetDirect = module.Offset(0x4920).RCast<MilesRouteSetMode_Type>();
    MilesRouteSetSpatialized = module.Offset(0x49A0).RCast<MilesRouteSetSpatialized_Type>();
    MilesRouteSetMixed = module.Offset(0x4A40).RCast<MilesRouteSetMixed_Type>();
    MilesSampleAddFilterByName = module.Offset(0x7A40).RCast<MilesSampleAddFilterByName_Type>();
    MilesFilterSetPropertyValueByName = module.Offset(0x83B0).RCast<MilesFilterSetPropertyValueByName_Type>();
    MilesFilterSetWetDryLevels = module.Offset(0x8460).RCast<MilesFilterSetWetDryLevels_Type>();
    MilesDriverGetMixedSamples = module.Offset(0x26D0).RCast<MilesDriverGetMixedSamples_Type>();
    MilesDriverGetSampleRate = module.Offset(0x2AF0).RCast<MilesDriverGetSampleRate_Type>();

    AudioHooks.DispatchForModule("mileswin64.dll");
})

ON_DLL_LOAD("server.dll", ServerCustomAudio, [](CModule module)
{
    NOTE_UNUSED(module);
    AudioHooks.DispatchForModule("server.dll");
})

ON_DLL_LOAD_RELIESON("engine.dll", MilesLogFuncHooks, ConVar, [](CModule module)
{ Cvar_mileslog_enable = new ConVar("mileslog_enable", "0", FCVAR_NONE, "Enables/disables whether the mileslog func should be logged"); })

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", AudioHooks, ConVar, [](CModule module)
{
    AudioHooks.DispatchForModule("client.dll");

    Cvar_ns_print_played_sounds = new ConVar("ns_print_played_sounds", "0", FCVAR_NONE, "");
    MilesStopAll = module.Offset(0x580850).RCast<MilesStopAll_Type>();
})
