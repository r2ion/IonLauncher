#include "audio_definition_internal.h"

#include "rapidjson/error/en.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <regex>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

uint64_t ClientSoundEventDefinition::HashEventName(std::string_view eventName)
{
    uint64_t hash = 0xCBF29CE484222325ull;

    for (unsigned char character : eventName)
    {
        if (character >= 'A' && character <= 'Z')
            character += 'a' - 'A';
        else if (character == '.')
            character = '_';

        hash *= 0x100000001B3ull;
        hash ^= character;
    }

    return hash;
}

ClientSoundEventDefinition::ClientSoundEventDefinition(std::string eventName) : m_eventName(std::move(eventName))
{
    const uint64_t eventHash = HashEventName(m_eventName);
    memcpy(m_data.data(), &eventHash, sizeof(eventHash));

    // The client definition stores a two-bit suffix mask for each of its
    // suffix slots at +0x18. Custom definitions ignore native suffix variants,
    // but all bits must be available so the client reaches its queue call for
    // every listener/language configuration.
    const uint64_t suffixMask = std::numeric_limits<uint64_t>::max();
    memcpy(m_data.data() + 0x18, &suffixMask, sizeof(suffixMask));
}

std::optional<float> CWaveFileReader::ReadDurationSeconds() const
{
    std::ifstream stream(m_path, std::ios::binary);
    std::array<uint8_t, 16> bytes{};
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), 12) || memcmp(bytes.data(), "RIFF", 4) != 0 || memcmp(bytes.data() + 8, "WAVE", 4) != 0)
    {
        return {};
    }

    uint32_t byteRate = 0;
    uint32_t dataSize = 0;
    while (stream.read(reinterpret_cast<char*>(bytes.data()), 8))
    {
        const uint32_t chunkSize = ReadUint32(bytes, 4);
        const std::streamoff nextChunkOffset = static_cast<std::streamoff>(chunkSize) + (chunkSize & 1u);
        if (memcmp(bytes.data(), "fmt ", 4) == 0)
        {
            if (chunkSize < 16 || !stream.read(reinterpret_cast<char*>(bytes.data()), 16))
                return {};

            const uint16_t format = ReadUint16(bytes, 0);
            if (format != 1 && format != 3 && format != 0xFFFE)
                return {};

            byteRate = ReadUint32(bytes, 8);
            stream.seekg(nextChunkOffset - 16, std::ios::cur);
        }
        else if (memcmp(bytes.data(), "data", 4) == 0)
        {
            dataSize = chunkSize;
            stream.seekg(nextChunkOffset, std::ios::cur);
        }
        else
        {
            stream.seekg(nextChunkOffset, std::ios::cur);
        }

        if (byteRate != 0 && dataSize != 0)
            return static_cast<float>(dataSize) / static_cast<float>(byteRate);
    }

    return {};
}

uint16_t CWaveFileReader::ReadUint16(const std::array<uint8_t, 16>& bytes, size_t offset)
{
    return static_cast<uint16_t>(bytes[offset]) | static_cast<uint16_t>(bytes[offset + 1]) << 8;
}

uint32_t CWaveFileReader::ReadUint32(const std::array<uint8_t, 16>& bytes, size_t offset)
{
    return static_cast<uint32_t>(bytes[offset]) | static_cast<uint32_t>(bytes[offset + 1]) << 8 | static_cast<uint32_t>(bytes[offset + 2]) << 16 |
           static_cast<uint32_t>(bytes[offset + 3]) << 24;
}

bool CModAudioDefinitionReader::ReadFiniteFloat(const AudioJsonValue& object, const char* memberName, float& output, bool required) const
{
    if (!object.HasMember(memberName))
    {
        if (required)
            spdlog::error("Failed reading audio override file {}: missing {} property", m_path.string(), memberName);
        return !required;
    }

    const AudioJsonValue& value = object[memberName];
    if (!value.IsNumber() || !std::isfinite(value.GetDouble()))
    {
        spdlog::error("Failed reading audio override file {}: {} property must be a finite number", m_path.string(), memberName);
        return false;
    }

    output = static_cast<float>(value.GetDouble());
    return true;
}

bool CModAudioDefinitionReader::ReadOptionalGraphType(const AudioJsonValue& object, const char* memberName, uint8_t& output) const
{
    if (!object.HasMember(memberName))
        return true;

    const AudioJsonValue& value = object[memberName];
    if (!value.IsUint() || value.GetUint() > std::numeric_limits<uint8_t>::max())
    {
        spdlog::error("Failed reading audio override file {}: {} property must be an integer between 0 and 255", m_path.string(), memberName);
        return false;
    }

    output = static_cast<uint8_t>(value.GetUint());
    return true;
}

bool CModAudioDefinitionReader::ReadVector3(const AudioJsonValue& object, const char* memberName, std::array<float, 3>& output) const
{
    if (!object.HasMember(memberName) || !object[memberName].IsArray() || object[memberName].Size() != output.size())
    {
        spdlog::error("Failed reading audio override file {}: {} property must be an array of three numbers", m_path.string(), memberName);
        return false;
    }

    for (rapidjson::SizeType index = 0; index < output.size(); ++index)
    {
        const AudioJsonValue& value = object[memberName][index];
        if (!value.IsNumber() || !std::isfinite(value.GetDouble()))
        {
            spdlog::error("Failed reading audio override file {}: {} property must contain finite numbers", m_path.string(), memberName);
            return false;
        }
        output[index] = static_cast<float>(value.GetDouble());
    }

    return true;
}

bool CModAudioDefinitionReader::ReadGraph(const AudioJsonValue& value, const char* xName, const char* yName, std::vector<AudioGraphPoint>& output,
                                          const char* propertyName) const
{
    if (!value.IsArray() || value.Size() < 2)
    {
        spdlog::error("Failed reading audio override file {}: {} property must be an array with at least two points", m_path.string(), propertyName);
        return false;
    }

    output.reserve(value.Size());
    for (const AudioJsonValue& pointJson : value.GetArray())
    {
        if (!pointJson.IsObject())
        {
            spdlog::error("Failed reading audio override file {}: every {} point must be an object", m_path.string(), propertyName);
            return false;
        }

        AudioGraphPoint point;
        if (!ReadFiniteFloat(pointJson, xName, point.X) || !ReadFiniteFloat(pointJson, yName, point.Y) ||
            !ReadFiniteFloat(pointJson, "IncomingTangentX", point.IncomingTangentX, false) ||
            !ReadFiniteFloat(pointJson, "IncomingTangentY", point.IncomingTangentY, false) ||
            !ReadFiniteFloat(pointJson, "OutgoingTangentX", point.OutgoingTangentX, false) ||
            !ReadFiniteFloat(pointJson, "OutgoingTangentY", point.OutgoingTangentY, false) ||
            !ReadOptionalGraphType(pointJson, "IncomingType", point.IncomingType) ||
            !ReadOptionalGraphType(pointJson, "OutgoingType", point.OutgoingType))
        {
            return false;
        }

        if (!output.empty() && point.X <= output.back().X)
        {
            spdlog::error("Failed reading audio override file {}: {} point {} values must be strictly increasing", m_path.string(), propertyName,
                          xName);
            return false;
        }

        output.push_back(point);
    }

    return true;
}

bool CModAudioDefinitionReader::ReadRoutes(const AudioJsonValue& routesJson, std::vector<AudioRouteDefinition>& routes,
                                           std::vector<AudioControllerBinding>& controllerBindings) const
{
    if (!routesJson.IsArray() || routesJson.Empty())
    {
        spdlog::error("Failed reading audio override file {}: Routes must be a non-empty array", m_path.string());
        return false;
    }

    for (const AudioJsonValue& routeJson : routesJson.GetArray())
    {
        if (!routeJson.IsObject() || !routeJson.HasMember("Bus") || !routeJson["Bus"].IsString() || routeJson["Bus"].GetStringLength() == 0 ||
            !routeJson.HasMember("Mode") || !routeJson["Mode"].IsString())
        {
            spdlog::error("Failed reading audio override file {}: every route needs non-empty Bus and Mode strings", m_path.string());
            return false;
        }

        AudioRouteDefinition route;
        route.Bus = routeJson["Bus"].GetString();
        const std::string mode = routeJson["Mode"].GetString();
        if (mode == "direct")
            route.Mode = AudioRouteMode::DIRECT;
        else if (mode == "panned")
            route.Mode = AudioRouteMode::PANNED;
        else if (mode == "spatialized")
            route.Mode = AudioRouteMode::SPATIALIZED;
        else if (mode == "mixed")
            route.Mode = AudioRouteMode::MIXED;
        else
        {
            spdlog::error("Failed reading audio override file {}: route Mode must be direct, panned, spatialized, or mixed", m_path.string());
            return false;
        }

        std::optional<std::string> volumeController;
        if (routeJson.HasMember("Volume"))
        {
            const AudioJsonValue& volumeJson = routeJson["Volume"];
            if (volumeJson.IsString())
            {
                if (volumeJson.GetStringLength() == 0)
                {
                    spdlog::error("Failed reading audio override file {}: route Volume controller name must not be empty", m_path.string());
                    return false;
                }
                volumeController = volumeJson.GetString();
                route.Volume = 1.0f;
            }
            else if (!volumeJson.IsNumber() || !std::isfinite(volumeJson.GetDouble()) || volumeJson.GetDouble() < 0.0)
            {
                spdlog::error(
                    "Failed reading audio override file {}: route Volume must be a non-negative finite number or a non-empty global controller name",
                    m_path.string());
                return false;
            }
            else
            {
                route.Volume = static_cast<float>(volumeJson.GetDouble());
            }
        }
        if (routeJson.HasMember("LFEVolume") && (!ReadFiniteFloat(routeJson, "LFEVolume", route.LFEVolume) || route.LFEVolume < 0.0f))
        {
            spdlog::error("Failed reading audio override file {}: route LFEVolume must be non-negative", m_path.string());
            return false;
        }

        if (routeJson.HasMember("Matrix"))
        {
            const AudioJsonValue& matrixJson = routeJson["Matrix"];
            if (!matrixJson.IsArray() || matrixJson.Empty())
            {
                spdlog::error("Failed reading audio override file {}: route Matrix must be a non-empty flat array", m_path.string());
                return false;
            }
            for (const AudioJsonValue& levelJson : matrixJson.GetArray())
            {
                if (!levelJson.IsNumber() || !std::isfinite(levelJson.GetDouble()))
                {
                    spdlog::error("Failed reading audio override file {}: every route Matrix level must be finite", m_path.string());
                    return false;
                }
                route.Matrix.push_back(static_cast<float>(levelJson.GetDouble()));
            }
        }
        if (route.Mode == AudioRouteMode::MIXED && route.Matrix.empty())
        {
            spdlog::error("Failed reading audio override file {}: mixed routes require Matrix", m_path.string());
            return false;
        }
        if (route.Mode != AudioRouteMode::MIXED && !route.Matrix.empty())
        {
            spdlog::error("Failed reading audio override file {}: route Matrix requires Mode mixed", m_path.string());
            return false;
        }

        routes.push_back(std::move(route));
        if (volumeController)
        {
            AudioControllerBinding binding;
            binding.Controller = std::move(*volumeController);
            binding.Source = AudioControllerSource::GLOBAL;
            binding.Target = AudioControllerTarget::ROUTE;
            binding.Property = AudioControllerProperty::ROUTE_VOLUME;
            binding.Operation = AudioControllerOperation::SET;
            binding.TargetIndex = static_cast<unsigned int>(routes.size() - 1);
            // Miles percentage controllers such as ReverbSmall and
            // ReverbLarge use a 0-100 range; route levels are linear 0-1.
            binding.Curve = {
                AudioGraphPoint{.X = 0.0f, .Y = 0.0f},
                AudioGraphPoint{.X = 100.0f, .Y = 1.0f},
            };
            controllerBindings.push_back(std::move(binding));
        }
    }
    return true;
}

bool CModAudioDefinitionReader::ReadControllerBindings(const AudioJsonValue& bindingsJson, std::vector<AudioControllerBinding>& bindings) const
{
    if (!bindingsJson.IsArray())
    {
        spdlog::error("Failed reading audio override file {}: ControllerBindings must be an array", m_path.string());
        return false;
    }

    for (const AudioJsonValue& bindingJson : bindingsJson.GetArray())
    {
        if (!bindingJson.IsObject() || !bindingJson.HasMember("Controller") || !bindingJson["Controller"].IsString() ||
            bindingJson["Controller"].GetStringLength() == 0 || !bindingJson.HasMember("Property") || !bindingJson["Property"].IsString() ||
            !bindingJson.HasMember("Curve"))
        {
            spdlog::error("Failed reading audio override file {}: every controller binding needs Controller, Property, and Curve", m_path.string());
            return false;
        }

        AudioControllerBinding binding;
        binding.Controller = bindingJson["Controller"].GetString();
        if (bindingJson.HasMember("Source"))
        {
            if (!bindingJson["Source"].IsString())
                return false;
            const std::string source = bindingJson["Source"].GetString();
            if (source == "global")
                binding.Source = AudioControllerSource::GLOBAL;
            else if (source == "event")
                binding.Source = AudioControllerSource::EVENT;
            else
            {
                spdlog::error("Failed reading audio override file {}: controller Source must be global or event", m_path.string());
                return false;
            }
        }

        if (bindingJson.HasMember("Target"))
        {
            if (!bindingJson["Target"].IsString())
                return false;
            const std::string target = bindingJson["Target"].GetString();
            if (target == "sample")
                binding.Target = AudioControllerTarget::SAMPLE;
            else if (target == "route")
                binding.Target = AudioControllerTarget::ROUTE;
            else if (target == "filter")
                binding.Target = AudioControllerTarget::FILTER;
            else
            {
                spdlog::error("Failed reading audio override file {}: controller Target must be sample, route, or filter", m_path.string());
                return false;
            }
        }
        if (bindingJson.HasMember("TargetIndex"))
        {
            if (!bindingJson["TargetIndex"].IsUint())
                return false;
            binding.TargetIndex = bindingJson["TargetIndex"].GetUint();
        }
        if (bindingJson.HasMember("MatrixIndex"))
        {
            if (!bindingJson["MatrixIndex"].IsUint())
                return false;
            binding.MatrixIndex = bindingJson["MatrixIndex"].GetUint();
        }

        const std::string property = bindingJson["Property"].GetString();
        binding.PropertyName = property;
        if (binding.Target == AudioControllerTarget::SAMPLE)
        {
            if (property == "volume")
                binding.Property = AudioControllerProperty::VOLUME;
            else if (property == "playback_rate")
                binding.Property = AudioControllerProperty::PLAYBACK_RATE;
            else if (property == "low_pass_cutoff")
                binding.Property = AudioControllerProperty::LOW_PASS_CUTOFF;
            else if (property == "lfe_volume")
                binding.Property = AudioControllerProperty::LFE_VOLUME;
            else if (property == "pan_360")
                binding.Property = AudioControllerProperty::PAN_360;
            else if (property == "pan_left_right")
                binding.Property = AudioControllerProperty::PAN_LEFT_RIGHT;
            else if (property == "pan_front_back")
                binding.Property = AudioControllerProperty::PAN_FRONT_BACK;
            else
            {
                spdlog::error("Failed reading audio override file {}: unsupported sample controller Property {}", m_path.string(), property);
                return false;
            }
        }
        else if (binding.Target == AudioControllerTarget::ROUTE)
        {
            if (property == "volume")
                binding.Property = AudioControllerProperty::ROUTE_VOLUME;
            else if (property == "lfe_volume")
                binding.Property = AudioControllerProperty::ROUTE_LFE_VOLUME;
            else if (property == "matrix")
                binding.Property = AudioControllerProperty::ROUTE_MATRIX;
            else
            {
                spdlog::error("Failed reading audio override file {}: route controller Property must be volume, lfe_volume, or matrix",
                              m_path.string());
                return false;
            }
        }
        else
        {
            if (property == "wet")
                binding.Property = AudioControllerProperty::FILTER_WET;
            else if (property == "dry")
                binding.Property = AudioControllerProperty::FILTER_DRY;
            else
                binding.Property = AudioControllerProperty::FILTER_PROPERTY;
        }

        if (bindingJson.HasMember("Operation"))
        {
            if (!bindingJson["Operation"].IsString())
                return false;
            const std::string operation = bindingJson["Operation"].GetString();
            if (operation == "multiply")
                binding.Operation = AudioControllerOperation::MULTIPLY;
            else if (operation == "add")
                binding.Operation = AudioControllerOperation::ADD;
            else if (operation == "set")
                binding.Operation = AudioControllerOperation::SET;
            else
            {
                spdlog::error("Failed reading audio override file {}: controller Operation must be multiply, add, or set", m_path.string());
                return false;
            }
        }
        if (bindingJson.HasMember("DefaultInput") && !ReadFiniteFloat(bindingJson, "DefaultInput", binding.DefaultInput))
            return false;
        if (!ReadGraph(bindingJson["Curve"], "Input", "Output", binding.Curve, "controller Curve"))
            return false;
        bindings.push_back(std::move(binding));
    }
    return true;
}

std::shared_ptr<AudioSourceSelectorDefinition> CModAudioDefinitionReader::ReadSelector(const AudioJsonValue& selectorJson) const
{
    auto selector = std::make_shared<AudioSourceSelectorDefinition>();
    if (selectorJson.IsString())
    {
        selector->Source = selectorJson.GetString();
        return selector;
    }
    if (!selectorJson.IsObject())
    {
        spdlog::error("Failed reading audio override file {}: selector choices must be strings or objects", m_path.string());
        return {};
    }
    if (selectorJson.HasMember("Weight"))
    {
        if (!ReadFiniteFloat(selectorJson, "Weight", selector->Weight) || selector->Weight < 0.0f)
            return {};
    }
    if (selectorJson.HasMember("Source"))
    {
        if (!selectorJson["Source"].IsString() || selectorJson["Source"].GetStringLength() == 0)
            return {};
        selector->Source = selectorJson["Source"].GetString();
        return selector;
    }
    if (!selectorJson.HasMember("Mode") || !selectorJson["Mode"].IsString() || !selectorJson.HasMember("Choices") ||
        !selectorJson["Choices"].IsArray() || selectorJson["Choices"].Empty())
    {
        spdlog::error("Failed reading audio override file {}: selector objects need Mode and non-empty Choices", m_path.string());
        return {};
    }
    const std::string mode = selectorJson["Mode"].GetString();
    if (mode == "sequential")
        selector->Mode = AudioSelectorMode::SEQUENTIAL;
    else if (mode == "random")
        selector->Mode = AudioSelectorMode::RANDOM;
    else if (mode == "random_no_repeat")
        selector->Mode = AudioSelectorMode::RANDOM_NO_REPEAT;
    else if (mode == "weighted_random")
        selector->Mode = AudioSelectorMode::WEIGHTED_RANDOM;
    else if (mode == "controller")
        selector->Mode = AudioSelectorMode::CONTROLLER;
    else
    {
        spdlog::error("Failed reading audio override file {}: unknown selector Mode {}", m_path.string(), mode);
        return {};
    }

    if (selector->Mode == AudioSelectorMode::CONTROLLER)
    {
        if (!selectorJson.HasMember("Controller") || !selectorJson["Controller"].IsString())
            return {};
        selector->Controller = selectorJson["Controller"].GetString();
        if (selectorJson.HasMember("SourceType"))
        {
            if (!selectorJson["SourceType"].IsString())
                return {};
            const std::string source = selectorJson["SourceType"].GetString();
            if (source == "global")
                selector->ControllerSource = AudioControllerSource::GLOBAL;
            else if (source != "event")
                return {};
        }
        if (selectorJson.HasMember("DefaultInput") && !ReadFiniteFloat(selectorJson, "DefaultInput", selector->DefaultInput))
            return {};
        if (selectorJson.HasMember("Curve") && !ReadGraph(selectorJson["Curve"], "Input", "Output", selector->Curve, "selector Curve"))
            return {};
    }

    for (const AudioJsonValue& choiceJson : selectorJson["Choices"].GetArray())
    {
        std::shared_ptr<AudioSourceSelectorDefinition> choice = ReadSelector(choiceJson);
        if (!choice)
            return {};
        selector->Choices.push_back(std::move(choice));
    }
    return selector;
}

bool CModAudioDefinitionReader::ReadFilters(const AudioJsonValue& filtersJson, std::vector<AudioFilterDefinition>& filters) const
{
    if (!filtersJson.IsArray())
        return false;
    for (const AudioJsonValue& filterJson : filtersJson.GetArray())
    {
        if (!filterJson.IsObject() || !filterJson.HasMember("Name") || !filterJson["Name"].IsString() || filterJson["Name"].GetStringLength() == 0)
            return false;
        AudioFilterDefinition filter;
        filter.Name = filterJson["Name"].GetString();
        if (filterJson.HasMember("Wet") && (!ReadFiniteFloat(filterJson, "Wet", filter.Wet) || filter.Wet < 0.0f))
            return false;
        if (filterJson.HasMember("Dry") && (!ReadFiniteFloat(filterJson, "Dry", filter.Dry) || filter.Dry < 0.0f))
            return false;
        if (filterJson.HasMember("Properties"))
        {
            const AudioJsonValue& propertiesJson = filterJson["Properties"];
            if (!propertiesJson.IsObject())
                return false;
            for (auto property = propertiesJson.MemberBegin(); property != propertiesJson.MemberEnd(); ++property)
            {
                if (!property->value.IsNumber() || !std::isfinite(property->value.GetDouble()))
                    return false;
                filter.Properties.push_back({property->name.GetString(), static_cast<float>(property->value.GetDouble())});
            }
        }
        filters.push_back(std::move(filter));
    }
    return true;
}

bool CModAudioDefinitionReader::ReadPanning(const AudioJsonValue& panJson, AudioPanningDefinition& panning) const
{
    if (!panJson.IsObject())
        return false;
    if (panJson.HasMember("Pan360Degrees"))
    {
        if (!ReadFiniteFloat(panJson, "Pan360Degrees", panning.Pan360Degrees))
            return false;
        panning.HasPan360 = true;
    }
    if (panJson.HasMember("LeftRight"))
    {
        if (!ReadFiniteFloat(panJson, "LeftRight", panning.LeftRight) || panning.LeftRight < -1.0f || panning.LeftRight > 1.0f)
            return false;
        panning.HasLeftRight = true;
    }
    if (panJson.HasMember("FrontBack"))
    {
        if (!ReadFiniteFloat(panJson, "FrontBack", panning.FrontBack) || panning.FrontBack < -1.0f || panning.FrontBack > 1.0f)
            return false;
        panning.HasFrontBack = true;
    }
    if (panJson.HasMember("Levels"))
    {
        const AudioJsonValue& levelsJson = panJson["Levels"];
        if (!levelsJson.IsArray() || levelsJson.Size() != panning.Levels.size())
            return false;
        for (rapidjson::SizeType index = 0; index < levelsJson.Size(); ++index)
        {
            if (!levelsJson[index].IsNumber() || !std::isfinite(levelsJson[index].GetDouble()))
                return false;
            panning.Levels[index] = static_cast<float>(levelsJson[index].GetDouble());
        }
        panning.HasLevels = true;
    }
    if ((panning.HasPan360 || panning.HasLevels) && (static_cast<unsigned int>(panning.HasPan360) + static_cast<unsigned int>(panning.HasLevels) +
                                                         static_cast<unsigned int>(panning.HasLeftRight || panning.HasFrontBack) >
                                                     1))
    {
        spdlog::error("Failed reading audio override file {}: Pan360Degrees, Levels, and LeftRight/FrontBack are mutually exclusive pan modes",
                      m_path.string());
        return false;
    }
    return true;
}

bool CModAudioDefinitionReader::ReadLayer(const AudioJsonValue& layerJson, AudioLayerDefinition& layer) const
{
    if (!layerJson.IsObject())
        return false;
    if (layerJson.HasMember("Name"))
    {
        if (!layerJson["Name"].IsString() || layerJson["Name"].GetStringLength() == 0)
            return false;
        layer.Name = layerJson["Name"].GetString();
    }

    layer.Routes = m_eventDefinition.Routes;
    layer.PlayCount = m_eventDefinition.PlayCount;
    layer.LoopStartSamples = m_eventDefinition.LoopStartSamples;
    layer.LoopEndSamples = m_eventDefinition.LoopEndSamples;
    layer.LowPassCutoff = -1.0f;

    if (layerJson.HasMember("Routes"))
    {
        layer.Routes.clear();
        if (!ReadRoutes(layerJson["Routes"], layer.Routes, layer.ControllerBindings))
            return false;
    }
    if (layerJson.HasMember("ControllerBindings") && !ReadControllerBindings(layerJson["ControllerBindings"], layer.ControllerBindings))
        return false;
    if (layerJson.HasMember("Filters") && !ReadFilters(layerJson["Filters"], layer.Filters))
        return false;
    if (layerJson.HasMember("Panning") && !ReadPanning(layerJson["Panning"], layer.Panning))
        return false;

    if (layerJson.HasMember("SourceMode"))
    {
        if (!layerJson["SourceMode"].IsString())
            return false;
        const std::string mode = layerJson["SourceMode"].GetString();
        if (mode == "memory" || mode == "buffered")
            layer.SourceMode = AudioSourceMode::MEMORY;
        else if (mode == "stream")
            layer.SourceMode = AudioSourceMode::STREAM;
        else
        {
            spdlog::error("Failed reading audio override file {}: layer SourceMode must be memory, buffered, or stream", m_path.string());
            return false;
        }
    }
    if (layerJson.HasMember("StreamBufferBytes"))
    {
        if (!layerJson["StreamBufferBytes"].IsUint() || layerJson["StreamBufferBytes"].GetUint() == 0)
            return false;
        layer.StreamBufferBytes = layerJson["StreamBufferBytes"].GetUint();
    }

    auto readNonNegativeUint = [&](const char* name, unsigned int& value)
    {
        if (!layerJson.HasMember(name))
            return true;
        if (!layerJson[name].IsUint())
            return false;
        value = layerJson[name].GetUint();
        return true;
    };
    if (!readNonNegativeUint("StartPositionSamples", layer.StartPositionSamples) || !readNonNegativeUint("StartDelayMs", layer.StartDelayMs))
        return false;
    if (layerJson.HasMember("PlayCount"))
    {
        if (!layerJson["PlayCount"].IsInt() || layerJson["PlayCount"].GetInt() < 0)
            return false;
        layer.PlayCount = layerJson["PlayCount"].GetInt();
    }
    if (layerJson.HasMember("LoopStartSamples"))
    {
        if (!layerJson["LoopStartSamples"].IsInt() || layerJson["LoopStartSamples"].GetInt() < 0)
            return false;
        layer.LoopStartSamples = layerJson["LoopStartSamples"].GetInt();
    }
    if (layerJson.HasMember("LoopEndSamples"))
    {
        if (!layerJson["LoopEndSamples"].IsInt() || layerJson["LoopEndSamples"].GetInt() < -1)
            return false;
        layer.LoopEndSamples = layerJson["LoopEndSamples"].GetInt();
    }

    auto readNonNegativeFloat = [&](const char* name, float& value)
    { return !layerJson.HasMember(name) || (ReadFiniteFloat(layerJson, name, value) && value >= 0.0f); };
    if (!readNonNegativeFloat("Volume", layer.Volume) || !readNonNegativeFloat("VolumeRandomDb", layer.VolumeRandomDb) ||
        !readNonNegativeFloat("LowPassCutoff", layer.LowPassCutoff) || layer.LowPassCutoff > 20000.0f ||
        !readNonNegativeFloat("LFEVolume", layer.LFEVolume) || !readNonNegativeFloat("DopplerFactor", layer.DopplerFactor) ||
        layer.DopplerFactor > 1.0f || !readNonNegativeFloat("MetersPerGameUnit", layer.MetersPerGameUnit))
        return false;
    if (layerJson.HasMember("PlaybackRate") && (!ReadFiniteFloat(layerJson, "PlaybackRate", layer.PlaybackRate) || layer.PlaybackRate <= 0.0f))
        return false;
    if (layerJson.HasMember("PitchSemitones") && !ReadFiniteFloat(layerJson, "PitchSemitones", layer.PitchSemitones))
        return false;
    if (layerJson.HasMember("PitchRandomSemitones") &&
        (!ReadFiniteFloat(layerJson, "PitchRandomSemitones", layer.PitchRandomSemitones) || layer.PitchRandomSemitones < 0.0f))
        return false;

    if (layer.LoopEndSamples != -1 && layer.LoopEndSamples <= layer.LoopStartSamples)
        return false;
    if ((layer.LoopStartSamples != 0 || layer.LoopEndSamples != -1) && layer.PlayCount == 1)
        return false;
    if (layer.Routes.empty())
    {
        spdlog::error("Failed reading audio override file {}: every layer needs Routes or inherited event Routes", m_path.string());
        return false;
    }

    if (layerJson.HasMember("Selector") && layerJson.HasMember("Sources"))
    {
        spdlog::error("Failed reading audio override file {}: a layer may define Selector or Sources, not both", m_path.string());
        return false;
    }
    if (layerJson.HasMember("Selector"))
    {
        layer.Selector = ReadSelector(layerJson["Selector"]);
        if (!layer.Selector)
            return false;
    }
    else if (layerJson.HasMember("Sources"))
    {
        const AudioJsonValue& sourcesJson = layerJson["Sources"];
        if (!sourcesJson.IsArray() || sourcesJson.Empty())
            return false;
        auto selector = std::make_shared<AudioSourceSelectorDefinition>();
        selector->Mode = AudioSelectorMode::SEQUENTIAL;
        if (layerJson.HasMember("SelectionStrategy"))
        {
            if (!layerJson["SelectionStrategy"].IsString())
                return false;
            const std::string strategy = layerJson["SelectionStrategy"].GetString();
            if (strategy == "sequential")
                selector->Mode = AudioSelectorMode::SEQUENTIAL;
            else if (strategy == "random")
                selector->Mode = AudioSelectorMode::RANDOM;
            else if (strategy == "random_no_repeat")
                selector->Mode = AudioSelectorMode::RANDOM_NO_REPEAT;
            else if (strategy == "weighted_random")
                selector->Mode = AudioSelectorMode::WEIGHTED_RANDOM;
            else
                return false;
        }
        for (const AudioJsonValue& sourceJson : sourcesJson.GetArray())
        {
            std::shared_ptr<AudioSourceSelectorDefinition> source = ReadSelector(sourceJson);
            if (!source || source->Mode != AudioSelectorMode::SOURCE)
                return false;
            selector->Choices.push_back(std::move(source));
        }
        layer.Selector = std::move(selector);
    }
    return true;
}

static std::string NormalizeAudioSourcePath(const fs::path& path)
{
    std::string normalized = path.generic_string();
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) { return static_cast<char>(tolower(character)); });
    return normalized;
}

static bool ResolveAudioSelectorSources(const std::shared_ptr<AudioSourceSelectorDefinition>& selector, const std::vector<AudioSampleData>& samples,
                                        const fs::path& samplesFolder, const fs::path& definitionPath)
{
    if (!selector)
        return false;
    if (selector->Mode == AudioSelectorMode::SOURCE)
    {
        if (selector->SourceIndex < samples.size())
            return true;
        const std::string requested = NormalizeAudioSourcePath(fs::path(selector->Source));
        size_t matchedIndex = std::numeric_limits<size_t>::max();
        for (size_t index = 0; index < samples.size(); ++index)
        {
            const std::string relative = NormalizeAudioSourcePath(fs::relative(samples[index].Path, samplesFolder));
            const std::string filename = NormalizeAudioSourcePath(samples[index].Path.filename());
            if (requested != relative && requested != filename)
                continue;
            if (matchedIndex != std::numeric_limits<size_t>::max())
            {
                spdlog::error("Failed reading audio override file {}: source {} is ambiguous; use its relative path", definitionPath.string(),
                              selector->Source);
                return false;
            }
            matchedIndex = index;
        }
        if (matchedIndex == std::numeric_limits<size_t>::max())
        {
            spdlog::error("Failed reading audio override file {}: source {} was not found", definitionPath.string(), selector->Source);
            return false;
        }
        selector->SourceIndex = matchedIndex;
        return true;
    }

    for (const std::shared_ptr<AudioSourceSelectorDefinition>& choice : selector->Choices)
    {
        if (!ResolveAudioSelectorSources(choice, samples, samplesFolder, definitionPath))
            return false;
    }
    return !selector->Choices.empty();
}

static std::shared_ptr<AudioSourceSelectorDefinition> CreateAllSourcesSelector(size_t sampleCount, AudioSelectionStrategy strategy)
{
    auto selector = std::make_shared<AudioSourceSelectorDefinition>();
    switch (strategy)
    {
    case AudioSelectionStrategy::RANDOM:
        selector->Mode = AudioSelectorMode::RANDOM;
        break;
    case AudioSelectionStrategy::RANDOM_NO_REPEAT:
        selector->Mode = AudioSelectorMode::RANDOM_NO_REPEAT;
        break;
    case AudioSelectionStrategy::WEIGHTED_RANDOM:
        selector->Mode = AudioSelectorMode::WEIGHTED_RANDOM;
        break;
    case AudioSelectionStrategy::SEQUENTIAL:
    default:
        selector->Mode = AudioSelectorMode::SEQUENTIAL;
        break;
    }
    for (size_t index = 0; index < sampleCount; ++index)
    {
        auto source = std::make_shared<AudioSourceSelectorDefinition>();
        source->SourceIndex = index;
        selector->Choices.push_back(std::move(source));
    }
    return selector;
}

static void CollectAudioSelectorSourceIndices(const std::shared_ptr<AudioSourceSelectorDefinition>& selector, std::vector<bool>& requiredSamples)
{
    if (!selector)
        return;
    if (selector->Mode == AudioSelectorMode::SOURCE)
    {
        if (selector->SourceIndex < requiredSamples.size())
            requiredSamples[selector->SourceIndex] = true;
        return;
    }

    for (const std::shared_ptr<AudioSourceSelectorDefinition>& choice : selector->Choices)
        CollectAudioSelectorSourceIndices(choice, requiredSamples);
}

ModAudioEventDefinition::ModAudioEventDefinition(const std::string& data, const fs::path& path, const std::vector<std::string>& registeredEvents,
                                                 bool loadAudioSamples)
{
    if (data.length() <= 0)
    {
        spdlog::error("Failed reading audio override file {}: file is empty", path.string());
        return;
    }

    fs::path samplesFolder = path;
    samplesFolder = samplesFolder.replace_extension();

    if (!fs::exists(samplesFolder))
    {
        spdlog::error("Failed reading audio override file {}: samples folder doesn't exist; should be named the same as the definition file without "
                      "JSON extension.",
                      path.string());
        return;
    }

    rapidjson_document dataJson;
    dataJson.Parse<rapidjson::ParseFlag::kParseCommentsFlag | rapidjson::ParseFlag::kParseTrailingCommasFlag>(data);

    // fail if parse error
    if (dataJson.HasParseError())
    {
        spdlog::error("Failed reading audio override file {}: encountered parse error \"{}\" at offset {}", path.string(),
                      GetParseError_En(dataJson.GetParseError()), dataJson.GetErrorOffset());
        return;
    }

    // fail if it's not a json obj (could be an array, string, etc)
    if (!dataJson.IsObject())
    {
        spdlog::error("Failed reading audio override file {}: file is not a JSON object", path.string());
        return;
    }

    CModAudioDefinitionReader reader(path, *this);

    // fail if no event ids given
    if (!dataJson.HasMember("EventId"))
    {
        spdlog::error("Failed reading audio override file {}: JSON object does not have the EventId property", path.string());
        return;
    }

    // array of event ids
    if (dataJson["EventId"].IsArray())
    {
        for (auto& eventId : dataJson["EventId"].GetArray())
        {
            if (!eventId.IsString())
            {
                spdlog::error("Failed reading audio override file {}: EventId array has a value of invalid type, all must be strings", path.string());
                return;
            }

            EventIds.push_back(eventId.GetString());
        }
    }
    // singular event id
    else if (dataJson["EventId"].IsString())
    {
        EventIds.push_back(dataJson["EventId"].GetString());
    }
    // incorrect type
    else
    {
        spdlog::error("Failed reading audio override file {}: EventId property is of invalid type (must be a string or an array of strings)",
                      path.string());
        return;
    }

    if (dataJson.HasMember("EventTemplate") || dataJson.HasMember("OutputBus"))
    {
        spdlog::error("Failed reading audio override file {}: EventTemplate and OutputBus were removed; define the event with Routes", path.string());
        return;
    }

    if (dataJson.HasMember("Loop"))
    {
        spdlog::error("Failed reading audio override file {}: Loop was removed; use PlayCount (zero loops indefinitely)", path.string());
        return;
    }

    bool hasCustomEventOptions = false;
    bool hasExplicitNetworkRadius = false;
    bool hasExplicitNetworkDuration = false;

    if (dataJson.HasMember("Routes"))
    {
        hasCustomEventOptions = true;
        if (!reader.ReadRoutes(dataJson["Routes"], Routes, ControllerBindings))
            return;
    }

    if (dataJson.HasMember("PlayCount"))
    {
        hasCustomEventOptions = true;
        if (!dataJson["PlayCount"].IsInt() || dataJson["PlayCount"].GetInt() < 0)
        {
            spdlog::error("Failed reading audio override file {}: PlayCount must be a non-negative integer", path.string());
            return;
        }

        PlayCount = dataJson["PlayCount"].GetInt();
    }

    if (dataJson.HasMember("LoopStartSamples"))
    {
        hasCustomEventOptions = true;
        if (!dataJson["LoopStartSamples"].IsInt() || dataJson["LoopStartSamples"].GetInt() < 0)
        {
            spdlog::error("Failed reading audio override file {}: LoopStartSamples property must be a non-negative integer", path.string());
            return;
        }

        LoopStartSamples = dataJson["LoopStartSamples"].GetInt();
    }

    if (dataJson.HasMember("LoopEndSamples"))
    {
        hasCustomEventOptions = true;
        if (!dataJson["LoopEndSamples"].IsInt() || dataJson["LoopEndSamples"].GetInt() < -1)
        {
            spdlog::error("Failed reading audio override file {}: LoopEndSamples property must be -1 or a non-negative integer", path.string());
            return;
        }

        LoopEndSamples = dataJson["LoopEndSamples"].GetInt();
    }

    if (dataJson.HasMember("Volume"))
    {
        hasCustomEventOptions = true;
        if (!reader.ReadFiniteFloat(dataJson, "Volume", Volume) || Volume < 0.0f)
        {
            spdlog::error("Failed reading audio override file {}: Volume property must be a non-negative finite number", path.string());
            return;
        }
    }

    if (dataJson.HasMember("PlaybackRate"))
    {
        hasCustomEventOptions = true;
        if (!reader.ReadFiniteFloat(dataJson, "PlaybackRate", PlaybackRate) || PlaybackRate <= 0.0f)
        {
            spdlog::error("Failed reading audio override file {}: PlaybackRate property must be greater than zero", path.string());
            return;
        }
    }

    if (dataJson.HasMember("LowPassCutoff"))
    {
        hasCustomEventOptions = true;
        if (!reader.ReadFiniteFloat(dataJson, "LowPassCutoff", LowPassCutoff) || LowPassCutoff < 0.0f || LowPassCutoff > 20000.0f)
        {
            spdlog::error("Failed reading audio override file {}: LowPassCutoff property must be between zero and 20000 Hz", path.string());
            return;
        }
    }

    if (dataJson.HasMember("FadeInMs"))
    {
        hasCustomEventOptions = true;
        if (!dataJson["FadeInMs"].IsUint())
        {
            spdlog::error("Failed reading audio override file {}: FadeInMs property must be an unsigned integer", path.string());
            return;
        }

        FadeInMs = dataJson["FadeInMs"].GetUint();
    }

    if (dataJson.HasMember("FadeOutMs"))
    {
        hasCustomEventOptions = true;
        if (!dataJson["FadeOutMs"].IsUint())
        {
            spdlog::error("Failed reading audio override file {}: FadeOutMs property must be an unsigned integer", path.string());
            return;
        }

        FadeOutMs = dataJson["FadeOutMs"].GetUint();
    }

    if (dataJson.HasMember("MaxInstances"))
    {
        hasCustomEventOptions = true;
        if (!dataJson["MaxInstances"].IsUint())
        {
            spdlog::error("Failed reading audio override file {}: MaxInstances property must be an unsigned integer", path.string());
            return;
        }

        MaxInstances = dataJson["MaxInstances"].GetUint();
    }

    if (dataJson.HasMember("InstanceLimitPolicy"))
    {
        hasCustomEventOptions = true;
        if (!dataJson["InstanceLimitPolicy"].IsString())
        {
            spdlog::error("Failed reading audio override file {}: InstanceLimitPolicy property must be a string", path.string());
            return;
        }

        const std::string policy = dataJson["InstanceLimitPolicy"].GetString();
        if (policy == "steal_oldest")
            InstanceLimitPolicy = AudioInstanceLimitPolicy::STEAL_OLDEST;
        else if (policy == "reject_new")
            InstanceLimitPolicy = AudioInstanceLimitPolicy::REJECT_NEW;
        else
        {
            spdlog::error("Failed reading audio override file {}: InstanceLimitPolicy must be steal_oldest or reject_new", path.string());
            return;
        }
    }

    if (dataJson.HasMember("Network"))
    {
        hasCustomEventOptions = true;
        const AudioJsonValue& networkJson = dataJson["Network"];
        if (!networkJson.IsObject())
        {
            spdlog::error("Failed reading audio override file {}: Network property must be an object", path.string());
            return;
        }

        if (networkJson.HasMember("Radius"))
        {
            if (!reader.ReadFiniteFloat(networkJson, "Radius", NetworkRadius) || NetworkRadius < 0.0f)
            {
                spdlog::error("Failed reading audio override file {}: Network Radius must be non-negative", path.string());
                return;
            }
            hasExplicitNetworkRadius = true;
        }

        if (networkJson.HasMember("DurationSeconds"))
        {
            if (!reader.ReadFiniteFloat(networkJson, "DurationSeconds", NetworkDurationSeconds) || NetworkDurationSeconds <= 0.0f)
            {
                spdlog::error("Failed reading audio override file {}: Network DurationSeconds must be greater than zero", path.string());
                return;
            }
            hasExplicitNetworkDuration = true;
        }

        if (networkJson.HasMember("SoundTags"))
        {
            if (!networkJson["SoundTags"].IsUint())
            {
                spdlog::error("Failed reading audio override file {}: Network SoundTags must be an unsigned integer", path.string());
                return;
            }
            SoundTags = networkJson["SoundTags"].GetUint();
        }
    }

    if (dataJson.HasMember("Spatialization"))
    {
        hasCustomEventOptions = true;
        const AudioJsonValue& spatialJson = dataJson["Spatialization"];
        if (!spatialJson.IsObject())
        {
            spdlog::error("Failed reading audio override file {}: Spatialization property must be an object", path.string());
            return;
        }

        if (spatialJson.HasMember("ListenerMask"))
        {
            if (!spatialJson["ListenerMask"].IsUint())
            {
                spdlog::error("Failed reading audio override file {}: ListenerMask property must be an unsigned integer", path.string());
                return;
            }
            Spatialization.HasListenerMask = true;
            Spatialization.ListenerMask = spatialJson["ListenerMask"].GetUint();
        }

        if (spatialJson.HasMember("AutoSpreadDistance"))
        {
            if (!reader.ReadFiniteFloat(spatialJson, "AutoSpreadDistance", Spatialization.AutoSpreadDistance) ||
                Spatialization.AutoSpreadDistance < 0.0f)
            {
                spdlog::error("Failed reading audio override file {}: AutoSpreadDistance must be non-negative", path.string());
                return;
            }
            Spatialization.HasAutoSpreadDistance = true;
        }

        if (spatialJson.HasMember("MultiChannelPan"))
        {
            const AudioJsonValue& panJson = spatialJson["MultiChannelPan"];
            if (!panJson.IsObject() || !reader.ReadFiniteFloat(panJson, "AngleDegrees", Spatialization.MultiChannelPanAngleDegrees) ||
                !reader.ReadFiniteFloat(panJson, "Distance", Spatialization.MultiChannelPanDistance) ||
                Spatialization.MultiChannelPanAngleDegrees < 0.0f || Spatialization.MultiChannelPanDistance < 0.0f)
            {
                spdlog::error("Failed reading audio override file {}: MultiChannelPan needs non-negative AngleDegrees and Distance", path.string());
                return;
            }
            Spatialization.HasMultiChannelPan = true;
        }

        if (spatialJson.HasMember("Orientation"))
        {
            const AudioJsonValue& orientationJson = spatialJson["Orientation"];
            if (!orientationJson.IsObject() || !reader.ReadVector3(orientationJson, "Facing", Spatialization.Facing) ||
                !reader.ReadVector3(orientationJson, "Up", Spatialization.Up))
            {
                spdlog::error("Failed reading audio override file {}: Orientation needs Facing and Up vectors", path.string());
                return;
            }

            const float facingLengthSquared =
                std::inner_product(Spatialization.Facing.begin(), Spatialization.Facing.end(), Spatialization.Facing.begin(), 0.0f);
            const float upLengthSquared = std::inner_product(Spatialization.Up.begin(), Spatialization.Up.end(), Spatialization.Up.begin(), 0.0f);
            if (facingLengthSquared <= 0.0f || upLengthSquared <= 0.0f)
            {
                spdlog::error("Failed reading audio override file {}: Orientation vectors must be non-zero", path.string());
                return;
            }
            Spatialization.HasOrientation = true;
        }

        if (spatialJson.HasMember("VolumeCone"))
        {
            const AudioJsonValue& coneJson = spatialJson["VolumeCone"];
            Spatialization.VolumeConeEnabled = true;
            if (!coneJson.IsObject())
            {
                spdlog::error("Failed reading audio override file {}: VolumeCone property must be an object", path.string());
                return;
            }

            if (coneJson.HasMember("Enabled"))
            {
                if (!coneJson["Enabled"].IsBool())
                {
                    spdlog::error("Failed reading audio override file {}: VolumeCone Enabled property must be a bool", path.string());
                    return;
                }
                Spatialization.VolumeConeEnabled = coneJson["Enabled"].GetBool();
            }

            if (!reader.ReadFiniteFloat(coneJson, "InnerAngleDegrees", Spatialization.VolumeConeInnerAngleDegrees) ||
                !reader.ReadFiniteFloat(coneJson, "OuterAngleDegrees", Spatialization.VolumeConeOuterAngleDegrees) ||
                !reader.ReadFiniteFloat(coneJson, "OuterVolume", Spatialization.VolumeConeOuterVolume) ||
                Spatialization.VolumeConeInnerAngleDegrees < 0.0f ||
                Spatialization.VolumeConeOuterAngleDegrees < Spatialization.VolumeConeInnerAngleDegrees ||
                Spatialization.VolumeConeOuterAngleDegrees > 360.0f || Spatialization.VolumeConeOuterVolume < 0.0f)
            {
                spdlog::error("Failed reading audio override file {}: invalid VolumeCone angles or OuterVolume", path.string());
                return;
            }
            Spatialization.HasVolumeCone = true;
        }

        if (spatialJson.HasMember("VolumeCurve") &&
            !reader.ReadGraph(spatialJson["VolumeCurve"], "Distance", "Value", Spatialization.VolumeCurve, "VolumeCurve"))
            return;
        if (spatialJson.HasMember("SpreadCurve") &&
            !reader.ReadGraph(spatialJson["SpreadCurve"], "Distance", "Value", Spatialization.SpreadCurve, "SpreadCurve"))
            return;
        if (spatialJson.HasMember("LowPassCurve") &&
            !reader.ReadGraph(spatialJson["LowPassCurve"], "Distance", "Value", Spatialization.LowPassCurve, "LowPassCurve"))
            return;
    }

    if (dataJson.HasMember("ControllerBindings"))
    {
        hasCustomEventOptions = true;
        if (!reader.ReadControllerBindings(dataJson["ControllerBindings"], ControllerBindings))
            return;
    }

    if (dataJson.HasMember("Layers"))
    {
        hasCustomEventOptions = true;
        const AudioJsonValue& layersJson = dataJson["Layers"];
        if (!layersJson.IsArray() || layersJson.Empty())
        {
            spdlog::error("Failed reading audio override file {}: Layers must be a non-empty array", path.string());
            return;
        }
        unsigned int layerIndex = 0;
        for (const AudioJsonValue& layerJson : layersJson.GetArray())
        {
            AudioLayerDefinition layer;
            layer.Name = "layer_" + std::to_string(layerIndex++);
            if (!reader.ReadLayer(layerJson, layer))
            {
                spdlog::error("Failed reading audio override file {}: invalid layer {}", path.string(), layer.Name);
                return;
            }
            Layers.push_back(std::move(layer));
        }
    }

    if (hasCustomEventOptions && Routes.empty() && Layers.empty())
    {
        spdlog::error("Failed reading audio override file {}: custom event properties require a non-empty Routes array", path.string());
        return;
    }

    const bool hasSpatializationOptions = Spatialization.HasListenerMask || Spatialization.HasAutoSpreadDistance ||
                                          Spatialization.HasMultiChannelPan || Spatialization.HasOrientation || Spatialization.HasVolumeCone ||
                                          !Spatialization.VolumeCurve.empty() || !Spatialization.SpreadCurve.empty() ||
                                          !Spatialization.LowPassCurve.empty();
    const bool hasSpatializedRoute =
        std::ranges::any_of(Routes, [](const AudioRouteDefinition& route) { return route.Mode == AudioRouteMode::SPATIALIZED; }) ||
        std::ranges::any_of(Layers, [](const AudioLayerDefinition& layer)
    { return std::ranges::any_of(layer.Routes, [](const AudioRouteDefinition& route) { return route.Mode == AudioRouteMode::SPATIALIZED; }); });
    if (hasSpatializationOptions && !hasSpatializedRoute)
    {
        spdlog::error("Failed reading audio override file {}: Spatialization options require at least one spatialized route", path.string());
        return;
    }

    if (!hasExplicitNetworkRadius && !Spatialization.VolumeCurve.empty())
        NetworkRadius = Spatialization.VolumeCurve.back().X;

    if (LoopEndSamples != -1 && LoopEndSamples <= LoopStartSamples)
    {
        spdlog::error("Failed reading audio override file {}: LoopEndSamples must be greater than LoopStartSamples", path.string());
        return;
    }

    if ((LoopStartSamples != 0 || LoopEndSamples != -1) && PlayCount == 1)
    {
        spdlog::error("Failed reading audio override file {}: loop points require PlayCount to be zero or greater than one", path.string());
        return;
    }

    if (dataJson.HasMember("EventIdRegex"))
    {
        // array of event id regex
        if (dataJson["EventIdRegex"].IsArray())
        {
            for (auto& eventId : dataJson["EventIdRegex"].GetArray())
            {
                if (!eventId.IsString())
                {
                    spdlog::error("Failed reading audio override file {}: EventIdRegex array has a value of invalid type, all must be strings",
                                  path.string());
                    return;
                }

                const std::string& regex = eventId.GetString();

                try
                {
                    EventIdsRegex.push_back({regex, std::regex(regex)});
                }
                catch (...)
                {
                    spdlog::error("Malformed regex \"{}\" in audio override file {}", regex, path.string());
                    return;
                }
            }
        }
        // singular event id regex
        else if (dataJson["EventIdRegex"].IsString())
        {
            const std::string& regex = dataJson["EventIdRegex"].GetString();
            try
            {
                EventIdsRegex.push_back({regex, std::regex(regex)});
            }
            catch (...)
            {
                spdlog::error("Malformed regex \"{}\" in audio override file {}", regex, path.string());
                return;
            }
        }
        // incorrect type
        else
        {
            spdlog::error("Failed reading audio override file {}: EventIdRegex property is of invalid type (must be a string or an array of strings)",
                          path.string());
            return;
        }
    }

    if (dataJson.HasMember("AudioSelectionStrategy"))
    {
        if (!dataJson["AudioSelectionStrategy"].IsString())
        {
            spdlog::error("Failed reading audio override file {}: AudioSelectionStrategy property must be a string", path.string());
            return;
        }

        std::string strategy = dataJson["AudioSelectionStrategy"].GetString();

        if (strategy == "sequential")
        {
            Strategy = AudioSelectionStrategy::SEQUENTIAL;
        }
        else if (strategy == "random")
        {
            Strategy = AudioSelectionStrategy::RANDOM;
        }
        else if (strategy == "random_no_repeat")
        {
            Strategy = AudioSelectionStrategy::RANDOM_NO_REPEAT;
        }
        else if (strategy == "weighted_random")
        {
            Strategy = AudioSelectionStrategy::WEIGHTED_RANDOM;
        }
        else
        {
            spdlog::error("Failed reading audio override file {}: unsupported AudioSelectionStrategy {}", path.string(), strategy);
            return;
        }
    }

    bool foundAudioSample = false;
    float longestWaveDuration = 0.0f;

    for (fs::directory_entry file : fs::recursive_directory_iterator(samplesFolder))
    {
        if (!file.is_regular_file())
            continue;

        std::string extension = file.path().extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        const bool isWave = extension == ".wav";
        const bool isFlac = extension == ".flac";
        if (isWave || isFlac)
        {
            foundAudioSample = true;
            if (isWave)
            {
                if (const std::optional<float> duration = CWaveFileReader(file.path()).ReadDurationSeconds())
                    longestWaveDuration = std::max(longestWaveDuration, *duration);
            }

            std::wstring pathString = file.path().wstring();

            // Retrieve event id from path (standard?)
            const fs::path eventFilename = file.path().parent_path().filename();
            std::string eventId = eventFilename.string();
            if (std::find(registeredEvents.begin(), registeredEvents.end(), eventId) != registeredEvents.end())
            {
                spdlog::warn(L"{} couldn't be loaded because {} event has already been overrided, skipping.", pathString, eventFilename.wstring());
                continue;
            }

            // Open the file.
            std::ifstream audioStream(pathString, std::ios::binary);

            if (audioStream.fail())
            {
                spdlog::error(L"Failed reading audio sample {}", pathString);
                continue;
            }

            // Get file size.
            audioStream.seekg(0, std::ios::end);
            size_t fileSize = audioStream.tellg();
            audioStream.close();

            Samples.push_back({.Path = file.path(), .Size = fileSize, .DecoderType = isFlac ? 3 : 64});
        }
    }

    if (!hasExplicitNetworkDuration && longestWaveDuration > 0.0f)
        NetworkDurationSeconds = longestWaveDuration;

    IsCustomEvent = !Routes.empty() || !Layers.empty();
    if (IsCustomEvent && Layers.empty())
    {
        AudioLayerDefinition layer;
        layer.Name = "main";
        layer.Routes = Routes;
        layer.PlayCount = PlayCount;
        layer.LoopStartSamples = LoopStartSamples;
        layer.LoopEndSamples = LoopEndSamples;
        layer.LowPassCutoff = -1.0f;
        layer.Selector = CreateAllSourcesSelector(Samples.size(), Strategy);
        Layers.push_back(std::move(layer));
    }
    for (AudioLayerDefinition& layer : Layers)
    {
        if (!layer.Selector)
            layer.Selector = CreateAllSourcesSelector(Samples.size(), AudioSelectionStrategy::SEQUENTIAL);
        else if (!ResolveAudioSelectorSources(layer.Selector, Samples, samplesFolder, path))
            return;
    }

    if (loadAudioSamples)
    {
        std::vector<bool> requiredSamples(Samples.size(), !IsCustomEvent);
        if (IsCustomEvent)
        {
            for (const AudioLayerDefinition& layer : Layers)
            {
                if (layer.SourceMode == AudioSourceMode::MEMORY)
                    CollectAudioSelectorSourceIndices(layer.Selector, requiredSamples);
            }
        }

        for (size_t sampleIndex = 0; sampleIndex < Samples.size(); ++sampleIndex)
        {
            if (!requiredSamples[sampleIndex])
                continue;

            AudioSampleData& sample = Samples[sampleIndex];
            const std::wstring pathString = sample.Path.wstring();
            std::ifstream audioStream(pathString, std::ios::binary);
            if (audioStream.fail())
            {
                spdlog::error(L"Failed reading audio sample {}", pathString);
                return;
            }

            sample.Data = std::make_unique<uint8_t[]>(sample.Size);
            audioStream.read(reinterpret_cast<char*>(sample.Data.get()), sample.Size);
            if (!audioStream)
            {
                spdlog::error(L"Failed reading complete audio sample {}", pathString);
                return;
            }
        }
    }

    if (!foundAudioSample || Samples.empty())
    {
        if (IsCustomEvent)
        {
            spdlog::error("Custom audio event {} has no valid WAV or FLAC samples", path.string());
            return;
        }

        spdlog::warn("Audio override {} has no valid WAV or FLAC samples; ignoring it.", path.string());
        return;
    }

    if (IsCustomEvent && !EventIdsRegex.empty())
    {
        spdlog::error("Custom audio event {} cannot use EventIdRegex; Routes require exact EventId values", path.string());
        return;
    }

    spdlog::info("Loaded audio override file {}", path.string());

    LoadedSuccessfully = true;
}
