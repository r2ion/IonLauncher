#pragma once

#include "audio.h"
#include "tier0/memstd.h"

#include <array>
#include <optional>
#include <utility>

using AudioJsonValue = rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>;

class CWaveFileReader
{
  public:
    explicit CWaveFileReader(fs::path path) : m_path(std::move(path))
    {
    }

    std::optional<float> ReadDurationSeconds() const;

  private:
    static uint16_t ReadUint16(const std::array<uint8_t, 16>& bytes, size_t offset);
    static uint32_t ReadUint32(const std::array<uint8_t, 16>& bytes, size_t offset);

    fs::path m_path;
};

class CModAudioDefinitionReader
{
  public:
    CModAudioDefinitionReader(const fs::path& path, const ModAudioEventDefinition& eventDefinition) : m_path(path), m_eventDefinition(eventDefinition)
    {
    }

    bool ReadFiniteFloat(const AudioJsonValue& object, const char* memberName, float& output, bool required = true) const;
    bool ReadOptionalGraphType(const AudioJsonValue& object, const char* memberName, uint8_t& output) const;
    bool ReadVector3(const AudioJsonValue& object, const char* memberName, std::array<float, 3>& output) const;
    bool ReadGraph(const AudioJsonValue& value, const char* xName, const char* yName, std::vector<AudioGraphPoint>& output,
                   const char* propertyName) const;
    bool ReadRoutes(const AudioJsonValue& routesJson, std::vector<AudioRouteDefinition>& routes,
                    std::vector<AudioControllerBinding>& controllerBindings) const;
    bool ReadControllerBindings(const AudioJsonValue& bindingsJson, std::vector<AudioControllerBinding>& bindings) const;
    std::shared_ptr<AudioSourceSelectorDefinition> ReadSelector(const AudioJsonValue& selectorJson) const;
    bool ReadFilters(const AudioJsonValue& filtersJson, std::vector<AudioFilterDefinition>& filters) const;
    bool ReadPanning(const AudioJsonValue& panJson, AudioPanningDefinition& panning) const;
    bool ReadLayer(const AudioJsonValue& layerJson, AudioLayerDefinition& layer) const;

  private:
    const fs::path& m_path;
    const ModAudioEventDefinition& m_eventDefinition;
};
