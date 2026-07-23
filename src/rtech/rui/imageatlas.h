#pragma once

#include "rtech/pakasset.h"
#include "rtech/rui/rui_image_atlas_types.h"
#include "tier0/module.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

struct RHashMapU32;

// Northstar-owned wrapper around one raw atlas slot. RuiImageAtlas remains an
// ABI POD because RTech uses the lower part of the global array as allocator
// backing; this class reserves an upper runtime slot and owns every record,
// descriptor, and GPU resource published through it.
class CImageAtlas final
{
public:
	struct Image
	{
		uint32_t m_NameHash = 0;
		uint32_t m_PosX = 0;
		uint32_t m_PosY = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint8_t m_Flags = 0;
	};

	CImageAtlas() = default;
	~CImageAtlas();

	CImageAtlas(const CImageAtlas&) = delete;
	CImageAtlas& operator=(const CImageAtlas&) = delete;
	CImageAtlas(CImageAtlas&&) = delete;
	CImageAtlas& operator=(CImageAtlas&&) = delete;

	bool Create(
		uint16_t width,
		uint16_t height,
		void* texture,
		std::span<const Image> images);
	// Restores missing descriptors displaced by a higher-priority native atlas.
	// Returns true when this atlas owns at least one live descriptor; hashes
	// already published by another atlas are left alone.
	bool EnsureRegistered();
	void Destroy();

	bool IsResident() const noexcept;
	std::optional<uint8_t> GetAtlasIndex() const noexcept;

	static bool AreRuntimeBindingsReady();
	static uint64_t HashAssetPath(const char* path);
	static uint32_t HashImagePath(const char* path);
	static const RuiImageAssetDescriptor* FindAssetDescriptor(uint32_t nameHash);
	static const RuiImageAssetDescriptor* GetAssetDescriptor(int32_t descriptorIndex) noexcept;
	static RuiImageAtlas* GetAtlas(uint8_t atlasIndex) noexcept;

	static void ConfigureAssetBinding(PakAssetBinding_s* binding);
	static void OnEngineLoaded(CModule module);

private:
	using CreateGpuBufferFn = uint32_t(*)(
		RuiImageAtlas* atlas,
		const RuiImageAtlasGpuRecord* records);
	using DestroyGpuBufferFn = void(*)(RuiImageAtlas* atlas);
	using FindDescriptorFn = RuiImageAssetDescriptor*(*)(
		RHashMapU32* map,
		uint32_t nameHash);
	using FindOrReserveDescriptorFn = void*(*)(
		RHashMapU32* map,
		uint32_t nameHash,
		uint8_t* reservedNewEntry);
	using RemoveDescriptorFn = uint32_t*(*)(RHashMapU32* map, uint32_t nameHash);
	using PakStringToGuidFn = uint64_t(*)(const char* path);

	static std::optional<uint8_t> ReserveAtlasSlot();
	static void ReleaseAtlasSlot(uint8_t atlasIndex);
	static RuiImageAssetDescriptor* FindDescriptor(uint32_t nameHash);
	static RuiImageAssetDescriptor* FindOrReserveDescriptor(
		uint32_t nameHash,
		uint8_t* reservedNewEntry);
	static bool HasFreeDescriptor();
	static void CommitReservedDescriptor();
	static uint32_t* RemoveDescriptor(uint32_t nameHash);
	static void ReplaceBoundAsset(
		void* boundAsset,
		const void* newHeader,
		const void* previousHeader);

	bool DescriptorStillOwned(size_t imageIndex) const;
	void UnregisterDescriptors();

	static bool s_LoaderConfigured;
	static CreateGpuBufferFn s_CreateGpuBuffer;
	static DestroyGpuBufferFn s_DestroyGpuBuffer;
	static FindDescriptorFn s_FindDescriptor;
	static FindOrReserveDescriptorFn s_FindOrReserveDescriptor;
	static RemoveDescriptorFn s_RemoveDescriptor;
	static PakStringToGuidFn s_PakStringToGuidAligned;
	static PakStringToGuidFn s_PakStringToGuidUnaligned;
	static RHashMapU32* s_DescriptorMap;
	static RuiImageAtlas s_Atlases[RUI_IMAGE_ATLAS_CAPACITY];
	// These process-lifetime allocations outlive atlases owned by globals in
	// other translation units during CRT teardown.
	static std::mutex* const s_AtlasSlotMutex;
	static std::array<bool, RUI_IMAGE_ATLAS_CAPACITY>* const s_OwnedAtlasSlots;

	std::vector<RuiImageAtlasEntry> m_AtlasEntries;
	std::vector<RuiImageDimensions> m_ImageDimensions;
	std::vector<RuiImageAtlasGpuRecord> m_GpuRecords;
	std::vector<RuiImageAtlasNameRecord> m_NameRecords;
	std::vector<RuiImageAssetDescriptor*> m_Descriptors;
	std::optional<uint8_t> m_AtlasIndex;
};
