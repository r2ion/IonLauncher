#include "rtech/rui/dynamic_imageatlas.h"
#include "rtech/rui/dynamic_imageatlas_internal.h"
#include "rtech/rui/imageatlas_internal.h"
#include "rtech/rui/rui_internal.h"
#include "rtech/pakfilesystem.h"
#include "materialsystem/cmaterialglue.h"
#include "dedicated/dedicated.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
std::mutex s_DynamicAtlasMutex;
std::array<bool, RUI_IMAGE_ATLAS_CAPACITY> s_DynamicAtlasSlots = {};
std::unordered_map<PakHandle_t, std::vector<std::unique_ptr<CDynamicImageAtlas>>> s_DynamicAtlasesByPak;
std::unordered_map<uint32_t, std::vector<CDynamicImageAtlas*>> s_DynamicAtlasesByImage;
std::unordered_map<int32_t, std::unique_ptr<CDynamicImageAtlas>> s_ScriptDynamicAtlases;
int32_t s_NextScriptDynamicAtlasHandle = 1;

uint64_t GetRuiAssetGuid(const char* path)
{
	return RuiImageAtlas_HashAssetPath(path);
}

uint32_t GetRuiImageHash(const char* path)
{
	const uint64_t guid = GetRuiAssetGuid(path);
	return static_cast<uint32_t>(guid) ^ static_cast<uint32_t>(guid >> 32);
}

uint32_t GetRuiImageHash(const std::string& path)
{
	return GetRuiImageHash(path.c_str());
}

RuiImageAssetDescriptor* FindRuiImageDescriptor(uint32_t nameHash)
{
	return RuiImageAtlas_FindAssetDescriptor(nameHash);
}

bool ParseDynamicAtlasImage(
	const rapidjson_document::ValueType& value,
	CDynamicImageAtlas::ImageDefinition& image,
	std::string& errorMessage)
{
	if (!value.IsObject() || !value.HasMember("path") || !value["path"].IsString()
		|| value["path"].GetStringLength() == 0)
	{
		errorMessage = "every atlas image requires a non-empty string member 'path'";
		return false;
	}

	image.m_Path.assign(value["path"].GetString(), value["path"].GetStringLength());
	auto readUint = [&](const char* memberName, uint32_t& output)
	{
		if (!value.HasMember(memberName) || !value[memberName].IsUint())
		{
			errorMessage = fmt::format(
				"image '{}' requires unsigned integer member '{}'",
				image.m_Path,
				memberName);
			return false;
		}

		output = value[memberName].GetUint();
		return true;
	};

	if (!readUint("posX", image.m_PosX) || !readUint("posY", image.m_PosY)
		|| !readUint("width", image.m_Width) || !readUint("height", image.m_Height))
	{
		return false;
	}

	if (image.m_Width == 0 || image.m_Height == 0 || image.m_Width > UINT16_MAX || image.m_Height > UINT16_MAX)
	{
		errorMessage = fmt::format(
			"image '{}' width and height must be between 1 and {}",
			image.m_Path,
			UINT16_MAX);
		return false;
	}

	if (value.HasMember("flags"))
	{
		if (!value["flags"].IsUint() || value["flags"].GetUint() > UINT8_MAX)
		{
			errorMessage = fmt::format(
				"image '{}' flags must be between 0 and {}",
				image.m_Path,
				UINT8_MAX);
			return false;
		}
		image.m_Flags = static_cast<uint8_t>(value["flags"].GetUint());
	}

	return true;
}

std::unique_ptr<CDynamicImageAtlas> ParseDynamicAtlasDefinition(
	const rapidjson_document::ValueType& value,
	const fs::path& sourcePath,
	PakHandle_t ownerHandle)
{
	if (!value.IsObject())
	{
		spdlog::error("RUI atlas sidecar '{}': each atlas must be a JSON object", sourcePath.string());
		return nullptr;
	}

	if (value.HasMember("$type") && (!value["$type"].IsString() || strcmp(value["$type"].GetString(), "uimg") != 0))
	{
		spdlog::error("RUI atlas sidecar '{}': '$type' must be 'uimg'", sourcePath.string());
		return nullptr;
	}

	if (!value.HasMember("atlas") || !value["atlas"].IsString() || value["atlas"].GetStringLength() == 0)
	{
		spdlog::error("RUI atlas sidecar '{}': each atlas requires a non-empty string member 'atlas'", sourcePath.string());
		return nullptr;
	}

	CDynamicImageAtlas::Definition definition;
	definition.m_OwnerHandle = ownerHandle;
	definition.m_SourcePath = sourcePath;
	definition.m_TexturePath.assign(value["atlas"].GetString(), value["atlas"].GetStringLength());

	if (!value.HasMember("textures"))
	{
		CDynamicImageAtlas::ImageDefinition image;
		image.m_Path = definition.m_TexturePath;
		image.m_UseFullTexture = true;
		definition.m_Images.push_back(std::move(image));
		return std::make_unique<CDynamicImageAtlas>(std::move(definition));
	}

	if (!value["textures"].IsArray() || value["textures"].Empty())
	{
		spdlog::error("RUI atlas sidecar '{}': 'textures' must be a non-empty array", sourcePath.string());
		return nullptr;
	}

	if (value["textures"].Size() > static_cast<rapidjson::SizeType>(INT16_MAX))
	{
		spdlog::error(
			"RUI atlas sidecar '{}': atlas '{}' contains too many images (maximum {})",
			sourcePath.string(),
			definition.m_TexturePath,
			INT16_MAX);
		return nullptr;
	}

	std::unordered_set<uint32_t> imageHashes;
	for (const auto& imageValue : value["textures"].GetArray())
	{
		CDynamicImageAtlas::ImageDefinition image;
		std::string errorMessage;
		if (!ParseDynamicAtlasImage(imageValue, image, errorMessage))
		{
			spdlog::error("RUI atlas sidecar '{}': {}", sourcePath.string(), errorMessage);
			return nullptr;
		}

		const uint32_t imageHash = GetRuiImageHash(image.m_Path);
		if (!imageHashes.insert(imageHash).second)
		{
			spdlog::error(
				"RUI atlas sidecar '{}': image '{}' collides with another image name in the same atlas",
				sourcePath.string(),
				image.m_Path);
			return nullptr;
		}

		definition.m_Images.push_back(std::move(image));
	}

	return std::make_unique<CDynamicImageAtlas>(std::move(definition));
}

std::vector<std::unique_ptr<CDynamicImageAtlas>> ParseDynamicAtlasSidecar(
	const fs::path& sourcePath,
	PakHandle_t ownerHandle)
{
	std::ifstream stream(sourcePath, std::ios::binary);
	if (!stream)
	{
		spdlog::error("Failed opening RUI atlas sidecar '{}'", sourcePath.string());
		return {};
	}

	std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	rapidjson_document document;
	document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(contents.c_str(), contents.size());
	if (document.HasParseError())
	{
		spdlog::error(
			"Failed parsing RUI atlas sidecar '{}': {} at byte {}",
			sourcePath.string(),
			rapidjson::GetParseError_En(document.GetParseError()),
			document.GetErrorOffset());
		return {};
	}

	std::vector<std::unique_ptr<CDynamicImageAtlas>> definitions;
	auto parseArray = [&](const rapidjson_document::ValueType& array)
	{
		for (const auto& atlasValue : array.GetArray())
		{
			auto definition = ParseDynamicAtlasDefinition(atlasValue, sourcePath, ownerHandle);
			if (definition)
				definitions.push_back(std::move(definition));
		}
	};

	if (document.IsArray())
	{
		parseArray(document);
	}
	else if (document.IsObject() && document.HasMember("atlases"))
	{
		if (!document["atlases"].IsArray())
		{
			spdlog::error("RUI atlas sidecar '{}': 'atlases' must be an array", sourcePath.string());
			return {};
		}
		parseArray(document["atlases"]);
	}
	else if (document.IsObject())
	{
		auto definition = ParseDynamicAtlasDefinition(document, sourcePath, ownerHandle);
		if (definition)
			definitions.push_back(std::move(definition));
	}
	else
	{
		spdlog::error("RUI atlas sidecar '{}': root must be an atlas object or array", sourcePath.string());
	}

	return definitions;
}

std::unique_ptr<CDynamicImageAtlas> ParseScriptDynamicAtlas(
	uint64_t textureGuid,
	const char* jsonData,
	std::string& errorMessage)
{
	if (!jsonData || !*jsonData)
	{
		errorMessage = "jsonData cannot be empty";
		return nullptr;
	}

	rapidjson_document document;
	document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(jsonData);
	if (document.HasParseError())
	{
		errorMessage = fmt::format(
			"invalid atlas JSON: {} at byte {}",
			rapidjson::GetParseError_En(document.GetParseError()),
			document.GetErrorOffset());
		return nullptr;
	}

	const rapidjson_document::ValueType* textures = nullptr;
	if (document.IsArray())
		textures = &document;
	else if (document.IsObject() && document.HasMember("textures"))
		textures = &document["textures"];

	if (!textures || !textures->IsArray() || textures->Empty())
	{
		errorMessage = "jsonData must be a non-empty image array or an object containing a non-empty 'textures' array";
		return nullptr;
	}
	if (textures->Size() > static_cast<rapidjson::SizeType>(INT16_MAX))
	{
		errorMessage = fmt::format("an atlas can contain at most {} images", INT16_MAX);
		return nullptr;
	}

	CDynamicImageAtlas::Definition definition;
	definition.m_SourcePath = "NS_CreateImageAtlas";
	definition.m_TexturePath = fmt::format("GUID 0x{:016X}", textureGuid);
	definition.m_TextureGuid = textureGuid;

	std::unordered_set<uint32_t> imageHashes;
	for (const auto& imageValue : textures->GetArray())
	{
		CDynamicImageAtlas::ImageDefinition image;
		if (!ParseDynamicAtlasImage(imageValue, image, errorMessage))
			return nullptr;

		const uint32_t imageHash = GetRuiImageHash(image.m_Path);
		if (!imageHashes.insert(imageHash).second)
		{
			errorMessage = fmt::format("image '{}' collides with another image name in the atlas", image.m_Path);
			return nullptr;
		}
		definition.m_Images.push_back(std::move(image));
	}

	return std::make_unique<CDynamicImageAtlas>(std::move(definition));
}

std::optional<fs::path> FindDynamicAtlasSidecar(const fs::path& pakPath)
{
	std::error_code error;
	fs::path appendedPath = pakPath;
	appendedPath += ".atlas.json";
	if (fs::is_regular_file(appendedPath, error))
		return appendedPath;

	error.clear();
	fs::path replacedPath = pakPath;
	replacedPath.replace_extension(".atlas.json");
	if (fs::is_regular_file(replacedPath, error))
		return replacedPath;

	return std::nullopt;
}

std::optional<uint8_t> AllocateDynamicAtlasSlot()
{
	for (int atlasIndex = RUI_DYNAMIC_IMAGE_ATLAS_LAST; atlasIndex >= RUI_DYNAMIC_IMAGE_ATLAS_FIRST; --atlasIndex)
	{
		if (s_DynamicAtlasSlots[atlasIndex])
			continue;

		s_DynamicAtlasSlots[atlasIndex] = true;
		return static_cast<uint8_t>(atlasIndex);
	}

	return std::nullopt;
}

void ReleaseDynamicAtlasSlot(uint8_t atlasIndex)
{
	if (atlasIndex >= RUI_DYNAMIC_IMAGE_ATLAS_FIRST && atlasIndex <= RUI_DYNAMIC_IMAGE_ATLAS_LAST)
		s_DynamicAtlasSlots[atlasIndex] = false;
}
}

CDynamicImageAtlas::CDynamicImageAtlas(Definition definition)
	: m_Definition(std::move(definition))
{
}

std::span<const CDynamicImageAtlas::ImageDefinition> CDynamicImageAtlas::GetImages() const noexcept
{
	return m_Definition.m_Images;
}

bool CDynamicImageAtlas::IsResident() const noexcept
{
	return m_State == State::Resident;
}

bool CDynamicImageAtlas::DescriptorStillOwned(const DescriptorRegistration& registration) const
{
	if (!m_AtlasIndex)
		return false;

	const RuiImageAssetDescriptor* descriptor = registration.m_Descriptor;
	return descriptor && descriptor->nameHash == registration.m_NameHash
		&& descriptor->imageIndex == registration.m_ImageIndex && descriptor->atlasIndex == *m_AtlasIndex;
}

void CDynamicImageAtlas::UnregisterDescriptors()
{
	if (!RuiImageAtlas_AreRuntimeBindingsReady() || !m_AtlasIndex)
	{
		m_Descriptors.clear();
		return;
	}

	AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	for (const DescriptorRegistration& registration : m_Descriptors)
	{
		if (DescriptorStillOwned(registration))
			RuiImageAtlas_RemoveExistingDescriptor(registration.m_NameHash);
	}
	ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	m_Descriptors.clear();
}

bool CDynamicImageAtlas::RegisterMissingDescriptors()
{
	if (!RuiImageAtlas_AreRuntimeBindingsReady() || !m_AtlasIndex)
		return false;

	bool addedDescriptor = false;
	AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	for (size_t imageIndex = 0; imageIndex < m_Definition.m_Images.size(); ++imageIndex)
	{
		const ImageDefinition& image = m_Definition.m_Images[imageIndex];
		const uint32_t nameHash = GetRuiImageHash(image.m_Path);
		if (RuiImageAtlas_FindAssetDescriptor(nameHash))
			continue;

		if (!RuiImageAtlas_HasFreeDescriptor())
		{
			spdlog::error(
				"RUI image descriptor table is full while loading '{}'",
				m_Definition.m_SourcePath.string());
			break;
		}

		uint8_t inserted = 0;
		RuiImageAssetDescriptor* descriptor =
			RuiImageAtlas_FindOrReserveDescriptorUnlocked(nameHash, &inserted);
		if (!descriptor || !inserted)
			continue;

		descriptor->nameHash = nameHash;
		descriptor->imageIndex = static_cast<int16_t>(imageIndex);
		descriptor->atlasIndex = *m_AtlasIndex;
		descriptor->flags = image.m_Flags;
		RuiImageAtlas_CommitReservedDescriptor();
		m_Descriptors.push_back(
			{nameHash, static_cast<int16_t>(imageIndex), descriptor});
		if (RuiImageAtlas_FindAssetDescriptor(nameHash) != descriptor)
		{
			spdlog::error(
				"RUI image '{}' descriptor insertion failed verification for hash 0x{:08X}",
				image.m_Path,
				nameHash);
			continue;
		}

		spdlog::info(
			"Registered RUI image '{}' with hash 0x{:08X} in runtime atlas {} at index {}",
			image.m_Path,
			nameHash,
			*m_AtlasIndex,
			imageIndex);
		addedDescriptor = true;
	}
	ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);

	return addedDescriptor;
}

void CDynamicImageAtlas::Destroy()
{
	UnregisterDescriptors();
	if (!m_AtlasIndex)
	{
		if (m_State == State::Resident)
			m_State = State::DefinitionOnly;
		return;
	}

	RuiImageAtlas& atlas = g_RuiImageAtlases[*m_AtlasIndex];
	if (atlas.gpuRecordBuffer != UINT_MAX)
		RuiImageAtlas_DestroyGpuBuffer(&atlas);

	atlas = {};

	ReleaseDynamicAtlasSlot(*m_AtlasIndex);
	m_AtlasIndex.reset();
	m_AtlasEntries.clear();
	m_ImageDimensions.clear();
	m_GpuRecords.clear();
	m_NameRecords.clear();
	if (m_State != State::Invalid)
		m_State = State::DefinitionOnly;
}

bool CDynamicImageAtlas::Create()
{
	if (m_State == State::Invalid)
		return false;
	if (m_State == State::Resident)
		return true;
	if (!RuiImageAtlas_AreRuntimeBindingsReady() || !g_pakLoadApi)
	{
		spdlog::error(
			"Cannot create runtime RUI atlas from '{}': the engine atlas loader is not ready",
			m_Definition.m_SourcePath.string());
		return false;
	}

	const uint64_t textureGuid = m_Definition.m_TextureGuid
		? *m_Definition.m_TextureGuid
		: GetRuiAssetGuid(m_Definition.m_TexturePath.c_str());
	auto* texture = reinterpret_cast<RpakTextureHeader*>(
		g_pakLoadApi->GetAssetBinding(textureGuid));
	if (!texture)
	{
		spdlog::error(
			"RUI atlas sidecar '{}': TXTR '{}' (GUID 0x{:016X}) is not loaded",
			m_Definition.m_SourcePath.string(),
			m_Definition.m_TexturePath,
			textureGuid);
		return false;
	}

	const uint32_t atlasWidth = texture->width;
	const uint32_t atlasHeight = texture->height;
	if (atlasWidth == 0 || atlasHeight == 0)
	{
		spdlog::error(
			"RUI atlas sidecar '{}': TXTR '{}' has invalid dimensions {}x{}",
			m_Definition.m_SourcePath.string(),
			m_Definition.m_TexturePath,
			atlasWidth,
			atlasHeight);
		return false;
	}

	std::vector<ImageDefinition> resolvedImages = m_Definition.m_Images;
	for (ImageDefinition& image : resolvedImages)
	{
		if (image.m_UseFullTexture)
		{
			image.m_Width = atlasWidth;
			image.m_Height = atlasHeight;
		}

		const uint64_t maxX = static_cast<uint64_t>(image.m_PosX) + image.m_Width;
		const uint64_t maxY = static_cast<uint64_t>(image.m_PosY) + image.m_Height;
		if (image.m_Width == 0 || image.m_Height == 0 || maxX > atlasWidth || maxY > atlasHeight)
		{
			spdlog::error(
				"RUI atlas sidecar '{}': image '{}' bounds ({}, {}, {}, {}) exceed texture '{}' dimensions {}x{}",
				m_Definition.m_SourcePath.string(),
				image.m_Path,
				image.m_PosX,
				image.m_PosY,
				image.m_Width,
				image.m_Height,
				m_Definition.m_TexturePath,
				atlasWidth,
				atlasHeight);
			m_State = State::Invalid;
			return false;
		}
	}

	const std::optional<uint8_t> atlasIndex = AllocateDynamicAtlasSlot();
	if (!atlasIndex)
	{
		spdlog::error(
			"No runtime RUI image atlas slots remain while loading '{}' ({} slots are reserved)",
			m_Definition.m_SourcePath.string(),
			RUI_IMAGE_ATLAS_CAPACITY - RUI_DYNAMIC_IMAGE_ATLAS_FIRST);
		return false;
	}

	m_Definition.m_Images = std::move(resolvedImages);
	m_AtlasEntries.resize(m_Definition.m_Images.size());
	m_ImageDimensions.resize(m_Definition.m_Images.size());
	m_GpuRecords.resize(m_Definition.m_Images.size());
	m_NameRecords.resize(m_Definition.m_Images.size());
	m_AtlasIndex = atlasIndex;

	const float inverseWidth = 1.0f / static_cast<float>(atlasWidth);
	const float inverseHeight = 1.0f / static_cast<float>(atlasHeight);
	for (size_t imageIndex = 0; imageIndex < m_Definition.m_Images.size(); ++imageIndex)
	{
		const ImageDefinition& image = m_Definition.m_Images[imageIndex];
		RuiImageAtlasEntry& atlasEntry = m_AtlasEntries[imageIndex];
		atlasEntry.pixelBounds[0] = 0.0f;
		atlasEntry.pixelBounds[1] = 0.0f;
		atlasEntry.pixelBounds[2] = static_cast<float>(image.m_Width);
		atlasEntry.pixelBounds[3] = static_cast<float>(image.m_Height);
		atlasEntry.uvBase[0] = static_cast<float>(image.m_PosX) * inverseWidth;
		atlasEntry.uvBase[1] = static_cast<float>(image.m_PosY) * inverseHeight;
		atlasEntry.uvScale[0] = inverseWidth;
		atlasEntry.uvScale[1] = inverseHeight;

		m_ImageDimensions[imageIndex] = {
			static_cast<uint16_t>(image.m_Width),
			static_cast<uint16_t>(image.m_Height)};

		RuiImageAtlasGpuRecord& gpuRecord = m_GpuRecords[imageIndex];
		gpuRecord.uvMin[0] = atlasEntry.uvBase[0];
		gpuRecord.uvMin[1] = atlasEntry.uvBase[1];
		gpuRecord.uvSize[0] = static_cast<float>(image.m_Width) * inverseWidth;
		gpuRecord.uvSize[1] = static_cast<float>(image.m_Height) * inverseHeight;

		RuiImageAtlasNameRecord& nameRecord = m_NameRecords[imageIndex];
		nameRecord.nameHash = GetRuiImageHash(image.m_Path);
		nameRecord.flags = image.m_Flags;
	}

	RuiImageAtlas& atlas = g_RuiImageAtlases[*atlasIndex];
	atlas = {};
	atlas.inverseWidth = inverseWidth;
	atlas.inverseHeight = inverseHeight;
	atlas.width = static_cast<uint16_t>(atlasWidth);
	atlas.height = static_cast<uint16_t>(atlasHeight);
	atlas.imageCount = static_cast<uint16_t>(m_Definition.m_Images.size());
	atlas.nineSliceImageCount = 0;
	atlas.images = m_AtlasEntries.data();
	atlas.imageDimensions = m_ImageDimensions.data();
	atlas.nineSliceData = nullptr;
	atlas.imageNameRecords = m_NameRecords.data();
	atlas.imageNames = nullptr;
	atlas.texture = texture;
	atlas.gpuRecordBuffer = UINT_MAX;

	RuiImageAtlas_CreateGpuBuffer(&atlas, m_GpuRecords.data());
	if (atlas.gpuRecordBuffer == UINT_MAX)
	{
		spdlog::error(
			"Failed creating the GPU bounds buffer for runtime RUI atlas '{}'",
			m_Definition.m_TexturePath);
		Destroy();
		return false;
	}

	if (!RegisterMissingDescriptors())
	{
		Destroy();
		return false;
	}
	m_State = State::Resident;

	spdlog::info(
		"Registered texture '{}' as runtime RUI atlas {} with {} image(s)",
		m_Definition.m_TexturePath,
		*atlasIndex,
		m_Definition.m_Images.size());
	return true;
}

bool CDynamicImageAtlas::EnsureResident()
{
	if (m_State == State::Invalid)
		return false;
	if (m_State != State::Resident && !Create())
		return false;

	RegisterMissingDescriptors();
	return true;
}

namespace
{
void IndexDynamicAtlasImages(CDynamicImageAtlas& atlas)
{
	for (const CDynamicImageAtlas::ImageDefinition& image : atlas.GetImages())
		s_DynamicAtlasesByImage[GetRuiImageHash(image.m_Path)].push_back(&atlas);
}

void UnindexDynamicAtlasImages(CDynamicImageAtlas& atlas)
{
	for (const CDynamicImageAtlas::ImageDefinition& image : atlas.GetImages())
	{
		const uint32_t nameHash = GetRuiImageHash(image.m_Path);
		auto imageIt = s_DynamicAtlasesByImage.find(nameHash);
		if (imageIt == s_DynamicAtlasesByImage.end())
			continue;

		auto& definitions = imageIt->second;
		definitions.erase(std::remove(definitions.begin(), definitions.end(), &atlas), definitions.end());
		if (definitions.empty())
			s_DynamicAtlasesByImage.erase(imageIt);
	}
}

std::optional<int32_t> AllocateScriptDynamicAtlasHandle()
{
	for (size_t attempt = 0; attempt <= s_ScriptDynamicAtlases.size(); ++attempt)
	{
		const int32_t candidate = s_NextScriptDynamicAtlasHandle;
		s_NextScriptDynamicAtlasHandle = candidate == INT32_MAX ? 1 : candidate + 1;
		if (!s_ScriptDynamicAtlases.contains(candidate))
			return candidate;
	}

	return std::nullopt;
}

void UnregisterDynamicAtlasesForPakLocked(PakHandle_t handle)
{
	auto ownerIt = s_DynamicAtlasesByPak.find(handle);
	if (ownerIt == s_DynamicAtlasesByPak.end())
		return;

	for (const auto& definition : ownerIt->second)
	{
		definition->Destroy();
		UnindexDynamicAtlasImages(*definition);
	}

	spdlog::info("Unregistered {} runtime RUI atlas definition(s) for pak handle {}", ownerIt->second.size(), handle);
	s_DynamicAtlasesByPak.erase(ownerIt);
}

bool TryRegisterDynamicAtlasForImage(const char* imagePath)
{
	if (!imagePath || !*imagePath || !RuiImageAtlas_AreRuntimeBindingsReady())
		return false;

	const uint32_t nameHash = GetRuiImageHash(imagePath);
	if (FindRuiImageDescriptor(nameHash))
		return true;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	auto imageIt = s_DynamicAtlasesByImage.find(nameHash);
	if (imageIt == s_DynamicAtlasesByImage.end())
		return false;

	spdlog::info(
		"Runtime RUI image lookup '{}' matched sidecar hash 0x{:08X}",
		imagePath,
		nameHash);

	for (auto definitionIt = imageIt->second.rbegin(); definitionIt != imageIt->second.rend(); ++definitionIt)
	{
		if ((*definitionIt)->EnsureResident() && FindRuiImageDescriptor(nameHash))
			return true;
	}

	spdlog::error(
		"Runtime RUI image '{}' matched a sidecar but descriptor registration failed for hash 0x{:08X}",
		imagePath,
		nameHash);
	return false;
}
}

bool RuiDynamicImageAtlas_TryRegisterImage(const char* imagePath)
{
	return TryRegisterDynamicAtlasForImage(imagePath);
}

std::optional<int32_t> RuiDynamicImageAtlas_Create(
	uint64_t textureGuid,
	const char* jsonData,
	std::string& errorMessage)
{
	auto definition = ParseScriptDynamicAtlas(textureGuid, jsonData, errorMessage);
	if (!definition)
		return std::nullopt;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	for (const CDynamicImageAtlas::ImageDefinition& image : definition->GetImages())
	{
		const uint32_t nameHash = GetRuiImageHash(image.m_Path);
		const auto reservedIt = s_DynamicAtlasesByImage.find(nameHash);
		if (FindRuiImageDescriptor(nameHash) || reservedIt != s_DynamicAtlasesByImage.end())
		{
			errorMessage = fmt::format(
				"cannot register RUI image '{}' because hash 0x{:08X} is already registered or reserved",
				image.m_Path,
				nameHash);
			return std::nullopt;
		}
	}

	if (!definition->EnsureResident())
	{
		errorMessage = fmt::format(
			"could not create an atlas for loaded TXTR GUID 0x{:016X}; check the log for the exact failure",
			textureGuid);
		return std::nullopt;
	}

	const std::optional<int32_t> handle = AllocateScriptDynamicAtlasHandle();
	if (!handle)
	{
		definition->Destroy();
		errorMessage = "could not allocate an atlas handle";
		return std::nullopt;
	}

	IndexDynamicAtlasImages(*definition);
	s_ScriptDynamicAtlases.emplace(*handle, std::move(definition));
	spdlog::info("Created script runtime RUI atlas handle {} for TXTR GUID 0x{:016X}", *handle, textureGuid);
	return handle;
}

bool RuiDynamicImageAtlas_Destroy(int32_t atlasHandle)
{
	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	auto atlasIt = s_ScriptDynamicAtlases.find(atlasHandle);
	if (atlasIt == s_ScriptDynamicAtlases.end())
		return false;

	atlasIt->second->Destroy();
	UnindexDynamicAtlasImages(*atlasIt->second);
	s_ScriptDynamicAtlases.erase(atlasIt);
	spdlog::info("Destroyed script runtime RUI atlas handle {}", atlasHandle);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Registers the atlas definition stored beside a pak before loading.
//-----------------------------------------------------------------------------
void RuiDynamicImageAtlas_OnPakLoaded(const fs::path& pakPath, PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE || IsDedicatedServer())
		return;

	const std::optional<fs::path> sidecarPath = FindDynamicAtlasSidecar(pakPath);
	if (!sidecarPath)
		return;

	auto definitions = ParseDynamicAtlasSidecar(*sidecarPath, handle);
	if (definitions.empty())
		return;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	UnregisterDynamicAtlasesForPakLocked(handle);
	for (const auto& definition : definitions)
		IndexDynamicAtlasImages(*definition);

	spdlog::info(
		"Loaded {} runtime RUI atlas definition(s) from '{}'",
		definitions.size(),
		sidecarPath->string());
	s_DynamicAtlasesByPak.emplace(handle, std::move(definitions));
}

//-----------------------------------------------------------------------------
// Purpose: Creates atlas wrappers once the pak's texture bindings are live.
//-----------------------------------------------------------------------------
void RuiDynamicImageAtlas_OnPakLoadCompleted(PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE || IsDedicatedServer())
		return;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	auto ownerIt = s_DynamicAtlasesByPak.find(handle);
	if (ownerIt == s_DynamicAtlasesByPak.end())
		return;

	for (const auto& definition : ownerIt->second)
		definition->EnsureResident();
}

//-----------------------------------------------------------------------------
// Purpose: Removes descriptors and GPU buffers before their texture pak unloads.
//-----------------------------------------------------------------------------
void RuiDynamicImageAtlas_OnPakUnloading(PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE)
		return;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	UnregisterDynamicAtlasesForPakLocked(handle);
}
