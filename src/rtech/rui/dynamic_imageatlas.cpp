#include "rtech/rui/dynamic_imageatlas.h"
#include "rtech/rui/rui_core_types.h"
#include "rtech/pakfilesystem.h"
#include "materialsystem/cmaterialglue.h"
#include "dedicated/dedicated.h"
#include "tier0/module.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
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

DECLARE_MODULE(RuiDynamicImageAtlasHooks)

std::mutex& CDynamicImageAtlas::s_Mutex = *new std::mutex;
std::unordered_map<PakHandle_t, std::vector<std::unique_ptr<CDynamicImageAtlas>>>&
	CDynamicImageAtlas::s_AtlasesByPak =
		*new std::unordered_map<PakHandle_t, std::vector<std::unique_ptr<CDynamicImageAtlas>>>;
std::unordered_map<uint32_t, std::vector<CDynamicImageAtlas*>>& CDynamicImageAtlas::s_AtlasesByImage =
	*new std::unordered_map<uint32_t, std::vector<CDynamicImageAtlas*>>;
std::unordered_map<CDynamicImageAtlas::Handle, std::unique_ptr<CDynamicImageAtlas>>&
	CDynamicImageAtlas::s_ScriptAtlases =
		*new std::unordered_map<CDynamicImageAtlas::Handle, std::unique_ptr<CDynamicImageAtlas>>;
CDynamicImageAtlas::Handle CDynamicImageAtlas::s_NextScriptHandle = 1;

DECLARE_HOOK(Rui_FindImageAsset, engine.dll + 0xF8000,
	[](auto& hook, RuiInstance* rui, const char* imagePath) -> RuiImageHandle
	{
		CDynamicImageAtlas::TryRegisterImage(imagePath);
		return hook.Original(rui, imagePath);
	})

bool CDynamicImageAtlas::ParseImage(
	const rapidjson_document::ValueType& value,
	Image& image,
	std::string& errorMessage)
{
	if (!value.IsObject() || !value.HasMember("path") || !value["path"].IsString()
		|| value["path"].GetStringLength() == 0)
	{
		errorMessage = "every atlas image requires a non-empty string member 'path'";
		return false;
	}

	image.m_Path.assign(value["path"].GetString(), value["path"].GetStringLength());
	image.m_NameHash = CImageAtlas::HashImagePath(image.m_Path.c_str());
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

std::unique_ptr<CDynamicImageAtlas> CDynamicImageAtlas::ParseDefinition(
	const rapidjson_document::ValueType& value,
	const fs::path& sourcePath)
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

	std::string texturePath(value["atlas"].GetString(), value["atlas"].GetStringLength());
	std::vector<Image> images;

	if (!value.HasMember("textures"))
	{
		Image image;
		image.m_Path = texturePath;
		image.m_NameHash = CImageAtlas::HashImagePath(image.m_Path.c_str());
		image.m_UseFullTexture = true;
		images.push_back(std::move(image));
		return std::unique_ptr<CDynamicImageAtlas>(
			new CDynamicImageAtlas(sourcePath, std::move(texturePath), std::nullopt, std::move(images)));
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
			texturePath,
			INT16_MAX);
		return nullptr;
	}

	std::unordered_set<uint32_t> imageHashes;
	for (const auto& imageValue : value["textures"].GetArray())
	{
		Image image;
		std::string errorMessage;
		if (!ParseImage(imageValue, image, errorMessage))
		{
			spdlog::error("RUI atlas sidecar '{}': {}", sourcePath.string(), errorMessage);
			return nullptr;
		}

		if (!imageHashes.insert(image.m_NameHash).second)
		{
			spdlog::error(
				"RUI atlas sidecar '{}': image '{}' collides with another image name in the same atlas",
				sourcePath.string(),
				image.m_Path);
			return nullptr;
		}

		images.push_back(std::move(image));
	}

	return std::unique_ptr<CDynamicImageAtlas>(
		new CDynamicImageAtlas(sourcePath, std::move(texturePath), std::nullopt, std::move(images)));
}

std::vector<std::unique_ptr<CDynamicImageAtlas>> CDynamicImageAtlas::ParseSidecar(
	const fs::path& sourcePath)
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
			auto definition = ParseDefinition(atlasValue, sourcePath);
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
		auto definition = ParseDefinition(document, sourcePath);
		if (definition)
			definitions.push_back(std::move(definition));
	}
	else
	{
		spdlog::error("RUI atlas sidecar '{}': root must be an atlas object or array", sourcePath.string());
	}

	return definitions;
}

std::unique_ptr<CDynamicImageAtlas> CDynamicImageAtlas::ParseScript(
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

	std::vector<Image> images;
	images.reserve(textures->Size());
	std::unordered_set<uint32_t> imageHashes;
	for (const auto& imageValue : textures->GetArray())
	{
		Image image;
		if (!ParseImage(imageValue, image, errorMessage))
			return nullptr;

		if (!imageHashes.insert(image.m_NameHash).second)
		{
			errorMessage = fmt::format("image '{}' collides with another image name in the atlas", image.m_Path);
			return nullptr;
		}
		images.push_back(std::move(image));
	}

	return std::unique_ptr<CDynamicImageAtlas>(
		new CDynamicImageAtlas(
			"NS_CreateImageAtlas",
			fmt::format("GUID 0x{:016X}", textureGuid),
			textureGuid,
			std::move(images)));
}

std::optional<fs::path> CDynamicImageAtlas::FindSidecar(const fs::path& pakPath)
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

CDynamicImageAtlas::CDynamicImageAtlas(
	fs::path sourcePath,
	std::string texturePath,
	std::optional<uint64_t> textureGuid,
	std::vector<Image> images)
	: m_SourcePath(std::move(sourcePath)),
	  m_TexturePath(std::move(texturePath)),
	  m_TextureGuid(textureGuid),
	  m_Images(std::move(images))
{
}

bool CDynamicImageAtlas::MakeResident()
{
	if (m_Invalid)
		return false;
	if (m_ImageAtlas.IsResident())
		return true;
	if (!CImageAtlas::AreRuntimeBindingsReady() || !g_pakLoadApi)
	{
		spdlog::error(
			"Cannot create runtime RUI atlas from '{}': the engine atlas loader is not ready",
			m_SourcePath.string());
		return false;
	}

	const uint64_t textureGuid = m_TextureGuid
		? *m_TextureGuid
		: CImageAtlas::HashAssetPath(m_TexturePath.c_str());
	// TODO: Track the actual TXTR binding with PakAssetBindingLink_s. This atlas
	// borrows the bound TXTR record; owner-handle teardown does not cover script
	// atlases, cross-pak textures, or provider fallback/unload. Listener setup
	// must be serialized with the Pak FIFO and rebuild UVs when dimensions change.
	auto* texture = reinterpret_cast<RpakTextureHeader*>(
		g_pakLoadApi->GetAssetBinding(textureGuid));
	if (!texture)
	{
		spdlog::error(
			"RUI atlas sidecar '{}': TXTR '{}' (GUID 0x{:016X}) is not loaded",
			m_SourcePath.string(),
			m_TexturePath,
			textureGuid);
		return false;
	}

	const uint32_t atlasWidth = texture->width;
	const uint32_t atlasHeight = texture->height;
	if (atlasWidth == 0 || atlasHeight == 0 || atlasWidth > UINT16_MAX || atlasHeight > UINT16_MAX)
	{
		spdlog::error(
			"RUI atlas sidecar '{}': TXTR '{}' has invalid dimensions {}x{}",
			m_SourcePath.string(),
			m_TexturePath,
			atlasWidth,
			atlasHeight);
		return false;
	}

	std::vector<Image> resolvedImages = m_Images;
	for (Image& image : resolvedImages)
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
				m_SourcePath.string(),
				image.m_Path,
				image.m_PosX,
				image.m_PosY,
				image.m_Width,
				image.m_Height,
				m_TexturePath,
				atlasWidth,
				atlasHeight);
			m_Invalid = true;
			return false;
		}
	}

	m_Images = std::move(resolvedImages);
	std::vector<CImageAtlas::Image> atlasImages;
	atlasImages.reserve(m_Images.size());
	for (const Image& image : m_Images)
	{
		atlasImages.push_back({
			image.m_NameHash,
			image.m_PosX,
			image.m_PosY,
			image.m_Width,
			image.m_Height,
			image.m_Flags});
	}

	if (!m_ImageAtlas.Create(
		static_cast<uint16_t>(atlasWidth),
		static_cast<uint16_t>(atlasHeight),
		texture,
		atlasImages))
	{
		spdlog::error(
			"Failed creating runtime RUI atlas '{}' (no slot, GPU buffer, or descriptor available)",
			m_TexturePath);
		return false;
	}

	const std::optional<uint8_t> atlasIndex = m_ImageAtlas.GetAtlasIndex();
	if (!atlasIndex)
	{
		m_ImageAtlas.Destroy();
		return false;
	}
	spdlog::info(
		"Registered texture '{}' as runtime RUI atlas {} with {} image(s)",
		m_TexturePath,
		*atlasIndex,
		m_Images.size());
	return true;
}

bool CDynamicImageAtlas::EnsureResident()
{
	if (m_Invalid)
		return false;
	if (!m_ImageAtlas.IsResident())
		return MakeResident();

	m_ImageAtlas.EnsureRegistered();
	return true;
}

std::optional<CDynamicImageAtlas::Handle> CDynamicImageAtlas::AllocateScriptHandle()
{
	for (size_t attempt = 0; attempt <= s_ScriptAtlases.size(); ++attempt)
	{
		const Handle candidate = s_NextScriptHandle;
		s_NextScriptHandle = candidate == INT32_MAX ? 1 : candidate + 1;
		if (!s_ScriptAtlases.contains(candidate))
			return candidate;
	}

	return std::nullopt;
}

void CDynamicImageAtlas::IndexImages(CDynamicImageAtlas& atlas)
{
	for (const Image& image : atlas.m_Images)
		s_AtlasesByImage[image.m_NameHash].push_back(&atlas);
}

void CDynamicImageAtlas::UnindexImages(CDynamicImageAtlas& atlas)
{
	for (const Image& image : atlas.m_Images)
	{
		auto imageIt = s_AtlasesByImage.find(image.m_NameHash);
		if (imageIt == s_AtlasesByImage.end())
			continue;

		auto& definitions = imageIt->second;
		definitions.erase(std::remove(definitions.begin(), definitions.end(), &atlas), definitions.end());
		if (definitions.empty())
			s_AtlasesByImage.erase(imageIt);
	}
}

void CDynamicImageAtlas::UnregisterForPakLocked(PakHandle_t handle)
{
	auto ownerIt = s_AtlasesByPak.find(handle);
	if (ownerIt == s_AtlasesByPak.end())
		return;

	for (const auto& definition : ownerIt->second)
		UnindexImages(*definition);

	spdlog::info("Unregistered {} runtime RUI atlas definition(s) for pak handle {}", ownerIt->second.size(), handle);
	s_AtlasesByPak.erase(ownerIt);
}

bool CDynamicImageAtlas::TryRegisterImage(const char* imagePath)
{
	if (!imagePath || !*imagePath || !CImageAtlas::AreRuntimeBindingsReady())
		return false;

	const uint32_t nameHash = CImageAtlas::HashImagePath(imagePath);
	if (CImageAtlas::FindAssetDescriptor(nameHash))
		return true;

	std::lock_guard<std::mutex> lock(s_Mutex);
	auto imageIt = s_AtlasesByImage.find(nameHash);
	if (imageIt == s_AtlasesByImage.end())
		return false;

	spdlog::info(
		"Runtime RUI image lookup '{}' matched sidecar hash 0x{:08X}",
		imagePath,
		nameHash);

	for (auto definitionIt = imageIt->second.rbegin(); definitionIt != imageIt->second.rend(); ++definitionIt)
	{
		if ((*definitionIt)->EnsureResident() && CImageAtlas::FindAssetDescriptor(nameHash))
			return true;
	}

	spdlog::error(
		"Runtime RUI image '{}' matched a sidecar but descriptor registration failed for hash 0x{:08X}",
		imagePath,
		nameHash);
	return false;
}

std::optional<CDynamicImageAtlas::Handle> CDynamicImageAtlas::Create(
	uint64_t textureGuid,
	const char* jsonData,
	std::string& errorMessage)
{
	auto definition = ParseScript(textureGuid, jsonData, errorMessage);
	if (!definition)
		return std::nullopt;

	std::lock_guard<std::mutex> lock(s_Mutex);
	for (const Image& image : definition->m_Images)
	{
		const auto reservedIt = s_AtlasesByImage.find(image.m_NameHash);
		if (CImageAtlas::FindAssetDescriptor(image.m_NameHash) || reservedIt != s_AtlasesByImage.end())
		{
			errorMessage = fmt::format(
				"cannot register RUI image '{}' because hash 0x{:08X} is already registered or reserved",
				image.m_Path,
				image.m_NameHash);
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

	const std::optional<Handle> handle = AllocateScriptHandle();
	if (!handle)
	{
		errorMessage = "could not allocate an atlas handle";
		return std::nullopt;
	}

	IndexImages(*definition);
	s_ScriptAtlases.emplace(*handle, std::move(definition));
	spdlog::info("Created script runtime RUI atlas handle {} for TXTR GUID 0x{:016X}", *handle, textureGuid);
	return handle;
}

bool CDynamicImageAtlas::Destroy(Handle atlasHandle)
{
	std::lock_guard<std::mutex> lock(s_Mutex);
	auto atlasIt = s_ScriptAtlases.find(atlasHandle);
	if (atlasIt == s_ScriptAtlases.end())
		return false;

	UnindexImages(*atlasIt->second);
	s_ScriptAtlases.erase(atlasIt);
	spdlog::info("Destroyed script runtime RUI atlas handle {}", atlasHandle);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Registers the atlas definition stored beside a pak before loading.
//-----------------------------------------------------------------------------
void CDynamicImageAtlas::OnPakLoaded(const fs::path& pakPath, PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE || IsDedicatedServer())
		return;

	const std::optional<fs::path> sidecarPath = FindSidecar(pakPath);
	if (!sidecarPath)
		return;

	auto definitions = ParseSidecar(*sidecarPath);
	if (definitions.empty())
		return;

	std::lock_guard<std::mutex> lock(s_Mutex);
	UnregisterForPakLocked(handle);
	for (const auto& definition : definitions)
		IndexImages(*definition);

	spdlog::info(
		"Loaded {} runtime RUI atlas definition(s) from '{}'",
		definitions.size(),
		sidecarPath->string());
	s_AtlasesByPak.emplace(handle, std::move(definitions));
}

//-----------------------------------------------------------------------------
// Purpose: Creates atlas wrappers once the pak's texture bindings are live.
//-----------------------------------------------------------------------------
void CDynamicImageAtlas::OnPakLoadCompleted(PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE || IsDedicatedServer())
		return;

	std::lock_guard<std::mutex> lock(s_Mutex);
	auto ownerIt = s_AtlasesByPak.find(handle);
	if (ownerIt == s_AtlasesByPak.end())
		return;

	for (const auto& definition : ownerIt->second)
		definition->EnsureResident();
}

//-----------------------------------------------------------------------------
// Purpose: Removes descriptors and GPU buffers before their texture pak unloads.
//-----------------------------------------------------------------------------
void CDynamicImageAtlas::OnPakUnloading(PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE)
		return;

	std::lock_guard<std::mutex> lock(s_Mutex);
	UnregisterForPakLocked(handle);
}

ON_DLL_LOAD("engine.dll", RuiDynamicImageAtlas, [](CModule module)
{
	// Keep this hook self-contained: the first lookup may happen immediately
	// after it is enabled, before another engine callback gets a turn.
	CImageAtlas::OnEngineLoaded(module);
	DISPATCH_MODULE(RuiDynamicImageAtlasHooks);
})
