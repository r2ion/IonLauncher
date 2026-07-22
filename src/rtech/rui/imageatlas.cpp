#include "rtech/rui/imageatlas_internal.h"
#include "rtech/rui/dynamic_imageatlas.h"
#include "rtech/rui/rui_internal.h"
#include "rtech/pakfilesystem.h"
#include "rtech/paktools.h"

#include <climits>
#include <cstdint>
#include <cstring>

DECLARE_MODULE(RuiImageAtlasHooks)

namespace
{
using CreateImageAtlasGpuBufferFn = uint32_t(__fastcall*)(
	RuiImageAtlas* atlas,
	const RuiImageAtlasGpuRecord* records);
using DestroyImageAtlasGpuBufferFn = void(__fastcall*)(RuiImageAtlas* atlas);
using FindImageAssetDescriptorFn = RuiImageAssetDescriptor*(__fastcall*)(
	RHashMapU32* hashMap,
	uint32_t nameHash);
using RpakHashFn = uint64_t(__fastcall*)(const char* path);

bool s_ImageAtlasLoaderConfigured = false;
CreateImageAtlasGpuBufferFn s_CreateImageAtlasGpuBuffer = nullptr;
DestroyImageAtlasGpuBufferFn s_DestroyImageAtlasGpuBuffer = nullptr;
FindImageAssetDescriptorFn s_FindImageAssetDescriptor = nullptr;
RHashMapU32FindOrReserveUnlockedFn s_RHashMapFindOrReserveUnlocked = nullptr;
RHashMapU32RemoveExistingFn s_RHashMapRemoveExisting = nullptr;
RpakHashFn s_RpakHashAligned = nullptr;
RpakHashFn s_RpakHashUnaligned = nullptr;

void __fastcall RuiImageAtlas_ReplaceBoundAsset(
	RuiImageAtlas* destination,
	const RuiImageAtlas* source,
	const RuiImageAtlas* previous)
{
	if (previous)
	{
		for (uint16_t imageIndex = 0; imageIndex < previous->imageCount; ++imageIndex)
			RuiImageAtlas_RemoveExistingDescriptor(previous->imageNameRecords[imageIndex].nameHash);
	}

	if (!source)
		return;

	*destination = *source;

	// The native replacement callback derives the descriptor atlas index from
	// the engine's 20-slot atlas array. The uimg binding below redirects storage
	// to this extended array, so its callback must use the same storage base.
	const uintptr_t destinationOffset = reinterpret_cast<uintptr_t>(destination)
		- reinterpret_cast<uintptr_t>(g_RuiImageAtlases);
	if (destinationOffset >= sizeof(g_RuiImageAtlases)
		|| destinationOffset % sizeof(RuiImageAtlas) != 0)
	{
		return;
	}

	const uint8_t atlasIndex = static_cast<uint8_t>(destinationOffset / sizeof(RuiImageAtlas));
	AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	for (uint16_t imageIndex = 0; imageIndex < source->imageCount; ++imageIndex)
	{
		const RuiImageAtlasNameRecord& nameRecord = source->imageNameRecords[imageIndex];
		uint8_t reservedNewEntry = 0;
		RuiImageAssetDescriptor* descriptor =
			RuiImageAtlas_FindOrReserveDescriptorUnlocked(nameRecord.nameHash, &reservedNewEntry);

		descriptor->nameHash = nameRecord.nameHash;
		descriptor->imageIndex = static_cast<int16_t>(imageIndex);
		descriptor->atlasIndex = atlasIndex;
		descriptor->flags = static_cast<uint8_t>(nameRecord.flags);
		RuiImageAtlas_CommitReservedDescriptor();
	}
	ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
}
}

DECLARE_HOOK(
	Pak_RegisterAssetBindingType,
	rtech_game.DLL + 0x7BE0,
	[](auto& hook, PakAssetBinding_s* binding, JobPriority_e priority, uint32_t affinity) -> JobTypeID_t
	{
		if (std::memcmp(binding->type, "uimg", sizeof(binding->type)) == 0)
		{
			// Native atlases use the lower range. The upper range is reserved for
			// runtime wrappers around ordinary texture assets.
			binding->assetCapacity = RUI_DYNAMIC_IMAGE_ATLAS_FIRST;
			binding->assetStorage = g_RuiImageAtlases;
			binding->replaceAssetFunc = reinterpret_cast<PakAssetReplaceFn_t>(
				RuiImageAtlas_ReplaceBoundAsset);
			s_ImageAtlasLoaderConfigured = true;
		}

		return hook.Original(binding, priority, affinity);
	})

DECLARE_HOOK(Rui_FindImageAsset, engine.dll + 0xF8000,
	[](auto& hook, RuiInstance* rui, const char* imagePath) -> RuiImageHandle
	{
		RuiDynamicImageAtlas_TryRegisterImage(imagePath);
		return hook.Original(rui, imagePath);
	})

bool RuiImageAtlas_AreRuntimeBindingsReady()
{
	return s_ImageAtlasLoaderConfigured && g_RuiImageDescriptorMap
		&& s_CreateImageAtlasGpuBuffer && s_DestroyImageAtlasGpuBuffer
		&& s_FindImageAssetDescriptor && s_RHashMapFindOrReserveUnlocked
		&& s_RHashMapRemoveExisting;
}

uint64_t RuiImageAtlas_HashAssetPath(const char* path)
{
	if (!path)
		return 0;

	RpakHashFn hashFunction = (reinterpret_cast<uintptr_t>(path) & 3)
		? s_RpakHashUnaligned
		: s_RpakHashAligned;
	return hashFunction ? hashFunction(path) : Pak_StringToGuid(path);
}

RuiImageAssetDescriptor* RuiImageAtlas_FindAssetDescriptor(uint32_t nameHash)
{
	if (!g_RuiImageDescriptorMap || !s_FindImageAssetDescriptor)
		return nullptr;

	return s_FindImageAssetDescriptor(g_RuiImageDescriptorMap, nameHash);
}

RuiImageAssetDescriptor* RuiImageAtlas_FindOrReserveDescriptorUnlocked(
	uint32_t nameHash,
	uint8_t* reservedNewEntry)
{
	if (!g_RuiImageDescriptorMap || !s_RHashMapFindOrReserveUnlocked)
		return nullptr;

	return static_cast<RuiImageAssetDescriptor*>(
		s_RHashMapFindOrReserveUnlocked(g_RuiImageDescriptorMap, nameHash, reservedNewEntry));
}

bool RuiImageAtlas_HasFreeDescriptor()
{
	return g_RuiImageDescriptorMap
		&& (g_RuiImageDescriptorMap->freeListHead != g_RuiImageDescriptorMap->nextUnusedIndex
			|| g_RuiImageDescriptorMap->nextUnusedIndex < g_RuiImageDescriptorMap->bucketPairCount);
}

void RuiImageAtlas_CommitReservedDescriptor()
{
	g_RuiImageDescriptorMap->bucketEntryIndices[g_RuiImageDescriptorMap->pendingBucketIndex]
		= static_cast<int32_t>(g_RuiImageDescriptorMap->pendingEntryIndex);
	++g_RuiImageDescriptorMap->liveEntryCount;
}

uint32_t* RuiImageAtlas_RemoveExistingDescriptor(uint32_t nameHash)
{
	if (!g_RuiImageDescriptorMap || !s_RHashMapRemoveExisting)
		return nullptr;

	return s_RHashMapRemoveExisting(g_RuiImageDescriptorMap, nameHash);
}

uint32_t RuiImageAtlas_CreateGpuBuffer(
	RuiImageAtlas* atlas,
	const RuiImageAtlasGpuRecord* records)
{
	return s_CreateImageAtlasGpuBuffer
		? s_CreateImageAtlasGpuBuffer(atlas, records)
		: UINT_MAX;
}

void RuiImageAtlas_DestroyGpuBuffer(RuiImageAtlas* atlas)
{
	if (s_DestroyImageAtlasGpuBuffer)
		s_DestroyImageAtlasGpuBuffer(atlas);
}

void RuiImageAtlas_OnEngineLoaded(CModule module)
{
	s_RpakHashAligned = module.Offset(0x4305D0).RCast<RpakHashFn>();
	s_RpakHashUnaligned = module.Offset(0x4305E0).RCast<RpakHashFn>();
	s_CreateImageAtlasGpuBuffer = module.Offset(0xFBF60).RCast<CreateImageAtlasGpuBufferFn>();
	s_DestroyImageAtlasGpuBuffer = module.Offset(0xFC4F0).RCast<DestroyImageAtlasGpuBufferFn>();
	s_FindImageAssetDescriptor = module.Offset(0xF3C60).RCast<FindImageAssetDescriptorFn>();
	s_RHashMapFindOrReserveUnlocked = module.Offset(0xF3BB0).RCast<RHashMapU32FindOrReserveUnlockedFn>();
	s_RHashMapRemoveExisting = module.Offset(0xF3E30).RCast<RHashMapU32RemoveExistingFn>();

	RuiImageAtlasHooks.DispatchForModule("engine.dll");
}

void RuiImageAtlas_DispatchRtechHooks()
{
	RuiImageAtlasHooks.DispatchForModule("rtech_game.dll");
}
