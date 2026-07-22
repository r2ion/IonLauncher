#pragma once

#include "rtech/pakasset.h"
#include "rtech/rui/rui_image_atlas_types.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Northstar-owned runtime wrapper for one dynamically constructed native RUI
// image atlas. The registry serializes lifecycle calls and must destroy an
// atlas while the native RUI bindings and its borrowed TXTR are still valid.
class CDynamicImageAtlas final
{
public:
	struct ImageDefinition
	{
		std::string m_Path;
		uint32_t m_PosX = 0;
		uint32_t m_PosY = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint8_t m_Flags = 0;
		bool m_UseFullTexture = false;
	};

	struct Definition
	{
		PakHandle_t m_OwnerHandle = PAK_INVALID_HANDLE;
		std::filesystem::path m_SourcePath;
		std::string m_TexturePath;
		std::optional<uint64_t> m_TextureGuid;
		std::vector<ImageDefinition> m_Images;
	};

	explicit CDynamicImageAtlas(Definition definition);
	~CDynamicImageAtlas() = default;

	CDynamicImageAtlas(const CDynamicImageAtlas&) = delete;
	CDynamicImageAtlas& operator=(const CDynamicImageAtlas&) = delete;
	CDynamicImageAtlas(CDynamicImageAtlas&&) = delete;
	CDynamicImageAtlas& operator=(CDynamicImageAtlas&&) = delete;

	std::span<const ImageDefinition> GetImages() const noexcept;
	bool IsResident() const noexcept;

	bool EnsureResident();
	void Destroy();

private:
	enum class State : uint8_t
	{
		DefinitionOnly,
		Resident,
		Invalid,
	};

	struct DescriptorRegistration
	{
		uint32_t m_NameHash;
		int16_t m_ImageIndex;
		RuiImageAssetDescriptor* m_Descriptor;
	};

	bool Create();
	bool RegisterMissingDescriptors();
	void UnregisterDescriptors();
	bool DescriptorStillOwned(const DescriptorRegistration& registration) const;

	Definition m_Definition;
	std::vector<RuiImageAtlasEntry> m_AtlasEntries;
	std::vector<RuiImageDimensions> m_ImageDimensions;
	std::vector<RuiImageAtlasGpuRecord> m_GpuRecords;
	std::vector<RuiImageAtlasNameRecord> m_NameRecords;
	std::vector<DescriptorRegistration> m_Descriptors;
	std::optional<uint8_t> m_AtlasIndex;
	State m_State = State::DefinitionOnly;
};
