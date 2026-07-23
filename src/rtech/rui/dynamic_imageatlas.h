#pragma once

#include "rtech/pakasset.h"
#include "rtech/rui/imageatlas.h"
#include "tier0/memstd.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Owns one dynamically constructed RUI atlas and the process-wide registry API
// used by pak sidecars, scripts, and lazy native image lookup.
class CDynamicImageAtlas final
{
public:
	using Handle = int32_t;

	CDynamicImageAtlas(const CDynamicImageAtlas&) = delete;
	CDynamicImageAtlas& operator=(const CDynamicImageAtlas&) = delete;
	CDynamicImageAtlas(CDynamicImageAtlas&&) = delete;
	CDynamicImageAtlas& operator=(CDynamicImageAtlas&&) = delete;

	// Script-created atlases are owned by an opaque handle until explicitly
	// destroyed. Their source TXTR must remain loaded for that lifetime.
	// Error details are returned to the Squirrel wrapper.
	static std::optional<Handle> Create(
		uint64_t textureGuid,
		const char* jsonData,
		std::string& errorMessage);
	static bool Destroy(Handle atlasHandle);

	// Registers definitions beside a pak, realizes them after TXTR bindings are
	// live, and tears them down with the pak that owns the sidecar definition.
	static void OnPakLoaded(const std::filesystem::path& pakPath, PakHandle_t handle);
	static void OnPakLoadCompleted(PakHandle_t handle);
	static void OnPakUnloading(PakHandle_t handle);

	// Called by the native image lookup hook before the engine resolves a name.
	static bool TryRegisterImage(const char* imagePath);

private:
	struct Image
	{
		std::string m_Path;
		uint32_t m_NameHash = 0;
		uint32_t m_PosX = 0;
		uint32_t m_PosY = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint8_t m_Flags = 0;
		bool m_UseFullTexture = false;
	};

	explicit CDynamicImageAtlas(
		std::filesystem::path sourcePath,
		std::string texturePath,
		std::optional<uint64_t> textureGuid,
		std::vector<Image> images);

	bool EnsureResident();
	bool MakeResident();

	static bool ParseImage(
		const rapidjson_document::ValueType& value,
		Image& image,
		std::string& errorMessage);
	static std::unique_ptr<CDynamicImageAtlas> ParseDefinition(
		const rapidjson_document::ValueType& value,
		const std::filesystem::path& sourcePath);
	static std::vector<std::unique_ptr<CDynamicImageAtlas>> ParseSidecar(
		const std::filesystem::path& sourcePath);
	static std::unique_ptr<CDynamicImageAtlas> ParseScript(
		uint64_t textureGuid,
		const char* jsonData,
		std::string& errorMessage);
	static std::optional<std::filesystem::path> FindSidecar(
		const std::filesystem::path& pakPath);
	static std::optional<Handle> AllocateScriptHandle();
	static void IndexImages(CDynamicImageAtlas& atlas);
	static void UnindexImages(CDynamicImageAtlas& atlas);
	static void UnregisterForPakLocked(PakHandle_t handle);

	// These registries intentionally have process lifetime. Destroying resident
	// atlases from CRT teardown could call engine and renderer code under the
	// loader lock, after those services have already begun shutting down.
	static std::mutex& s_Mutex;
	static std::unordered_map<PakHandle_t, std::vector<std::unique_ptr<CDynamicImageAtlas>>>& s_AtlasesByPak;
	static std::unordered_map<uint32_t, std::vector<CDynamicImageAtlas*>>& s_AtlasesByImage;
	static std::unordered_map<Handle, std::unique_ptr<CDynamicImageAtlas>>& s_ScriptAtlases;
	static Handle s_NextScriptHandle;

	std::filesystem::path m_SourcePath;
	std::string m_TexturePath;
	std::optional<uint64_t> m_TextureGuid;
	std::vector<Image> m_Images;
	CImageAtlas m_ImageAtlas;
	bool m_Invalid = false;
};
