#include "rtech/rui/atlas.h"

#include "modsystem/modatlas.h"
#include "rtech/pakasset.h"
#include "rtech/pakfilesystem.h"
#include "rtech/paktools.h"
#include "rtech/rstdlib.h"
#include "tier0/jobthread.h"
#include "tier0/module.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

DECLARE_MODULE(RuiImageAtlasBindingHooks)

RuiImageAtlas* g_RuiImageAtlases;
RHashMapU32* g_RuiImageDescriptorMap;

RuiCreateImageAtlasGpuBuffer_t RuiImageAtlas_CreateGpuBuffer;
RuiDestroyImageAtlasGpuBuffer_t RuiImageAtlas_DestroyGpuBuffer;

struct RuiImageAtlasGpuMetadata
{
    uint64_t generation;
    std::vector<RuiImageAtlasGpuRecord> records;
};

std::shared_mutex g_RuiImageAtlasMutex;
std::array<RuiImageAtlas, RUI_PAK_IMAGE_ATLAS_CAPACITY> g_PakRuiImageAtlases;
std::deque<RuiImageAtlas> g_DynamicRuiImageAtlases;
std::array<RuiImageAtlasHandle, RUI_IMAGE_DESCRIPTOR_CAPACITY> g_RuiImageAtlasHandles;
std::mutex g_RuiImageAtlasGpuRecordsMutex;
std::unordered_map<uint32_t, RuiImageAtlasGpuMetadata> g_RuiImageAtlasGpuRecords;
uint64_t g_RuiImageAtlasGpuGeneration = 0;

bool RuiGetImageAtlasGpuRecord(const RuiImageAtlas& atlas, uint16_t imageIndex, RuiImageAtlasGpuRecord& record)
{
    std::scoped_lock lock(g_RuiImageAtlasGpuRecordsMutex);
    const auto found = g_RuiImageAtlasGpuRecords.find(atlas.gpuRecordBuffer);
    if (found == g_RuiImageAtlasGpuRecords.end() || imageIndex >= found->second.records.size())
        return false;
    record = found->second.records[imageIndex];
    return true;
}

uint64_t RuiGetImageAtlasGpuGeneration(const RuiImageAtlas& atlas)
{
    std::scoped_lock lock(g_RuiImageAtlasGpuRecordsMutex);
    const auto found = g_RuiImageAtlasGpuRecords.find(atlas.gpuRecordBuffer);
    return found != g_RuiImageAtlasGpuRecords.end() ? found->second.generation : 0;
}

DECLARE_HOOK(RuiImageAtlasCaptureGpuRecords, engine.dll + 0xFBF60,
             [](auto& hook, RuiImageAtlas* atlas, const RuiImageAtlasGpuRecord* records) -> uint32_t
{
    const uint32_t buffer = hook.Original(atlas, records);
    if (buffer != UINT32_MAX && records && atlas->imageNameRecords)
    {
        std::scoped_lock lock(g_RuiImageAtlasGpuRecordsMutex);
        auto& metadata = g_RuiImageAtlasGpuRecords[buffer];
        metadata.generation = ++g_RuiImageAtlasGpuGeneration;
        metadata.records.assign(records, records + atlas->imageCount);
    }
    return buffer;
})

DECLARE_HOOK(RuiImageAtlasReleaseGpuRecords, engine.dll + 0xFC4F0, [](auto& hook, RuiImageAtlas* atlas) -> void
{
    {
        std::scoped_lock lock(g_RuiImageAtlasGpuRecordsMutex);
        g_RuiImageAtlasGpuRecords.erase(atlas->gpuRecordBuffer);
    }
    hook.Original(atlas);
})

bool RuiHasDuplicateImageNames(const RuiImageAtlas& atlas)
{
    std::vector<uint32_t> hashes;
    hashes.reserve(atlas.imageCount);
    for (uint16_t imageIndex = 0; imageIndex < atlas.imageCount; ++imageIndex)
        hashes.push_back(atlas.imageNameRecords[imageIndex].nameHash);

    std::sort(hashes.begin(), hashes.end());
    return std::adjacent_find(hashes.begin(), hashes.end()) != hashes.end();
}

void RuiRemoveImageAtlasDescriptors(const RuiImageAtlas& atlas, RuiImageAtlasHandle atlasHandle)
{
    if (!atlas.imageNameRecords)
        return;

    const uint8_t atlasIndex = atlasHandle < RUI_NATIVE_IMAGE_ATLAS_CAPACITY ? static_cast<uint8_t>(atlasHandle) : RUI_IMAGE_ATLAS_INDEX_DYNAMIC;
    const auto* descriptorStorage = static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entryStorage);
    for (uint16_t imageIndex = 0; imageIndex < atlas.imageCount; ++imageIndex)
    {
        const uint32_t nameHash = atlas.imageNameRecords[imageIndex].nameHash;
        const auto* descriptor = static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->Find(nameHash));
        if (!descriptor || descriptor->imageIndex != static_cast<int16_t>(imageIndex) || descriptor->atlasIndex != atlasIndex)
            continue;

        const size_t descriptorIndex = static_cast<size_t>(descriptor - descriptorStorage);
        if (descriptorIndex >= g_RuiImageAtlasHandles.size() || g_RuiImageAtlasHandles[descriptorIndex] != atlasHandle)
            continue;

        if (atlasHandle >= RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE)
            g_RuiImageAtlasHandles[descriptorIndex] = RUI_INVALID_IMAGE_ATLAS;
        g_RuiImageDescriptorMap->RemoveExisting(nameHash);
    }
}

bool RuiPublishImageAtlasDescriptors(const RuiImageAtlas& atlas, RuiImageAtlasHandle atlasHandle, bool replaceExisting)
{
    if (atlasHandle == RUI_INVALID_IMAGE_ATLAS || !atlas.imageNameRecords || atlas.imageCount > INT16_MAX)
        return false;

    uint32_t newDescriptorCount = 0;
    for (uint16_t imageIndex = 0; imageIndex < atlas.imageCount; ++imageIndex)
    {
        const uint32_t nameHash = atlas.imageNameRecords[imageIndex].nameHash;
        if (g_RuiImageDescriptorMap->Find(nameHash))
        {
            if (!replaceExisting)
                return false;
        }
        else
        {
            ++newDescriptorCount;
        }
    }
    if (newDescriptorCount > g_RuiImageDescriptorMap->entryCapacity - g_RuiImageDescriptorMap->liveEntryCount)
        return false;

    const uint8_t atlasIndex = atlasHandle < RUI_NATIVE_IMAGE_ATLAS_CAPACITY ? static_cast<uint8_t>(atlasHandle) : RUI_IMAGE_ATLAS_INDEX_DYNAMIC;
    const auto* descriptorStorage = static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entryStorage);
    for (uint16_t imageIndex = 0; imageIndex < atlas.imageCount; ++imageIndex)
    {
        const RuiImageAtlasNameRecord& name = atlas.imageNameRecords[imageIndex];
        bool reserved = false;
        auto* descriptor = static_cast<RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->FindOrReserveUnlocked(name.nameHash, reserved));
        if (!descriptor)
            return false;

        const size_t descriptorIndex = static_cast<size_t>(descriptor - descriptorStorage);
        if (descriptorIndex >= g_RuiImageAtlasHandles.size())
            return false;

        *descriptor = {
            .nameHash = name.nameHash,
            .imageIndex = static_cast<int16_t>(imageIndex),
            .atlasIndex = atlasIndex,
            .flags = static_cast<uint8_t>(name.flags),
        };
        if (reserved)
            g_RuiImageDescriptorMap->PublishReserved();
        g_RuiImageAtlasHandles[descriptorIndex] = atlasHandle;
    }
    return true;
}

bool RuiCommitImageAtlas(RuiImageAtlasHandle handle, const RuiImageAtlas& contents, bool publishNames)
{
    RuiImageAtlas* destination =
        handle < RUI_PAK_IMAGE_ATLAS_CAPACITY ? &g_PakRuiImageAtlases[handle] : RuiGetImageAtlas(handle);
    if (!destination || !destination->images || !contents.imageNameRecords || contents.imageCount > INT16_MAX)
        return false;
    if (!publishNames)
    {
        *destination = contents;
        if (handle < RUI_NATIVE_IMAGE_ATLAS_CAPACITY)
            g_RuiImageAtlases[handle] = contents;
        return true;
    }

    std::unordered_set<uint32_t> names;
    names.reserve(contents.imageCount);
    for (uint16_t index = 0; index < contents.imageCount; ++index)
        if (!names.insert(contents.imageNameRecords[index].nameHash).second)
            return false;

    AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
    if (!RuiPublishImageAtlasDescriptors(contents, handle, true))
    {
        ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
        return false;
    }

	const RuiImageAtlas previous = *destination;
    for (uint16_t index = 0; index < previous.imageCount; ++index)
    {
        const auto& name = previous.imageNameRecords[index];
        if (names.contains(name.nameHash))
            continue;
        const auto* descriptor = static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->Find(name.nameHash));
        const auto* storage = static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entryStorage);
        if (descriptor && g_RuiImageAtlasHandles[descriptor - storage] == handle)
            g_RuiImageDescriptorMap->RemoveExisting(name.nameHash);
    }
    *destination = contents;
    if (handle < RUI_NATIVE_IMAGE_ATLAS_CAPACITY)
        g_RuiImageAtlases[handle] = contents;
    ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
    return true;
}

RuiImageAtlasHandle RuiRegisterImageAtlas(const RuiImageAtlas& source, std::span<const RuiImageAtlasGpuRecord> records)
{
    if (!source.texture || !source.images || !source.imageDimensions || !source.imageNameRecords || source.imageCount == 0 ||
        source.imageCount > INT16_MAX || source.nineSliceImageCount != 0 || records.size() != source.imageCount || RuiHasDuplicateImageNames(source))
    {
        return RUI_INVALID_IMAGE_ATLAS;
    }

    RuiImageAtlas atlas = source;
    atlas.gpuRecordBuffer = UINT32_MAX;
    const uint32_t gpuBuffer = RuiImageAtlas_CreateGpuBuffer(&atlas, records.data());
    if (gpuBuffer == UINT32_MAX || atlas.gpuRecordBuffer != gpuBuffer)
        return RUI_INVALID_IMAGE_ATLAS;

    std::unique_lock lock(g_RuiImageAtlasMutex);
    if (g_DynamicRuiImageAtlases.size() >= RUI_INVALID_IMAGE_ATLAS - RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE)
    {
        RuiImageAtlas_DestroyGpuBuffer(&atlas);
        return RUI_INVALID_IMAGE_ATLAS;
    }

    const RuiImageAtlasHandle atlasHandle = RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE + static_cast<RuiImageAtlasHandle>(g_DynamicRuiImageAtlases.size());
    g_DynamicRuiImageAtlases.push_back(atlas);

    AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
    const bool published = RuiPublishImageAtlasDescriptors(g_DynamicRuiImageAtlases.back(), atlasHandle, false);
    ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
    if (!published)
    {
        RuiImageAtlas_DestroyGpuBuffer(&g_DynamicRuiImageAtlases.back());
        g_DynamicRuiImageAtlases.pop_back();
        return RUI_INVALID_IMAGE_ATLAS;
    }

    lock.unlock();
    return atlasHandle;
}

void RuiUnregisterImageAtlas(RuiImageAtlasHandle atlasHandle)
{
    if (atlasHandle < RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE || atlasHandle == RUI_INVALID_IMAGE_ATLAS)
        return;

    std::unique_lock lock(g_RuiImageAtlasMutex);
    const size_t atlasIndex = static_cast<size_t>(atlasHandle - RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE);
    if (atlasIndex >= g_DynamicRuiImageAtlases.size() || !g_DynamicRuiImageAtlases[atlasIndex].images)
        return;

    RuiImageAtlas& atlas = g_DynamicRuiImageAtlases[atlasIndex];
    AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
    RuiRemoveImageAtlasDescriptors(atlas, atlasHandle);
    ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);

    RuiImageAtlas_DestroyGpuBuffer(&atlas);
    atlas = {};
    lock.unlock();
}

RuiImageAtlas* RuiGetImageAtlas(RuiImageAtlasHandle atlasHandle)
{
    if (atlasHandle == RUI_INVALID_IMAGE_ATLAS)
        return nullptr;

    RuiImageAtlas* atlas = nullptr;
    if (atlasHandle < RUI_NATIVE_IMAGE_ATLAS_CAPACITY)
    {
        atlas = &g_RuiImageAtlases[atlasHandle];
    }
    else if (atlasHandle < RUI_PAK_IMAGE_ATLAS_CAPACITY)
    {
        atlas = &g_PakRuiImageAtlases[atlasHandle];
    }
    else
    {
        const size_t atlasIndex = static_cast<size_t>(atlasHandle - RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE);
        if (atlasIndex >= g_DynamicRuiImageAtlases.size())
            return nullptr;
        atlas = &g_DynamicRuiImageAtlases[atlasIndex];
    }
    return atlas->images ? atlas : nullptr;
}

RuiImageAtlasHandle RuiGetImageAtlasHandle(RuiImageHandle imageHandle)
{
    if (imageHandle < 0 || static_cast<uint32_t>(imageHandle) >= g_RuiImageDescriptorMap->entryCapacity)
        return RUI_INVALID_IMAGE_ATLAS;

    const auto* descriptor = &static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entryStorage)[imageHandle];
    if (descriptor->imageIndex < 0)
        return RUI_INVALID_IMAGE_ATLAS;

    const RuiImageAtlasHandle atlasHandle = descriptor->atlasIndex < RUI_NATIVE_IMAGE_ATLAS_CAPACITY
                                                ? static_cast<RuiImageAtlasHandle>(descriptor->atlasIndex)
                                                : g_RuiImageAtlasHandles[imageHandle];
    return RuiGetImageAtlas(atlasHandle) ? atlasHandle : RUI_INVALID_IMAGE_ATLAS;
}

bool RuiResolveImageAsset(RuiImageHandle imageHandle, RuiResolvedImageAsset& asset)
{
    const RuiImageAtlasHandle atlasHandle = RuiGetImageAtlasHandle(imageHandle);
    RuiImageAtlas* atlas = RuiGetImageAtlas(atlasHandle);
    if (!atlas)
        return false;

    const auto* descriptor = &static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entryStorage)[imageHandle];
    if (descriptor->imageIndex < 0 || static_cast<uint16_t>(descriptor->imageIndex) >= atlas->imageCount)
        return false;

    asset = {
        .atlas = atlas,
        .atlasHandle = atlasHandle,
        .imageIndex = descriptor->imageIndex,
        .flags = descriptor->flags,
    };
    return true;
}

bool RuiIsDynamicImageAsset(RuiImageHandle imageHandle)
{
    const RuiImageAtlasHandle atlasHandle = RuiGetImageAtlasHandle(imageHandle);
    return atlasHandle != RUI_INVALID_IMAGE_ATLAS && atlasHandle >= RUI_NATIVE_IMAGE_ATLAS_CAPACITY;
}

bool RuiResolvePakImageAtlas(const char* pakPath, const char* atlasPath, RuiImageAtlasHandle& handle)
{
    if (!atlasPath || !*atlasPath || !g_pakLoadApi)
        return false;
    PakGlobalState_s* globals = Pak_GetGlobals();
    if (!globals)
        return false;
    const PakGuid_t guid = Pak_StringToGuid(atlasPath);
    const uintptr_t bound = reinterpret_cast<uintptr_t>(g_pakLoadApi->GetAssetBinding(guid));
    const uintptr_t storage = reinterpret_cast<uintptr_t>(g_PakRuiImageAtlases.data());
    if (bound < storage || bound - storage >= sizeof(g_PakRuiImageAtlases) || (bound - storage) % sizeof(RuiImageAtlas) != 0)
        return false;
    const auto atlasHandle = static_cast<RuiImageAtlasHandle>((bound - storage) / sizeof(RuiImageAtlas));
    if (!RuiGetImageAtlas(atlasHandle))
        return false;

    for (const PakAssetBinding_s& binding : globals->assetBindings)
    {
        if (binding.assetStorage != g_PakRuiImageAtlases.data() || !binding.assetSlots || atlasHandle >= binding.assetCapacity)
            continue;
        const uint32_t assetIndex = binding.assetSlots[atlasHandle].loadedAssetIndex;
        if (assetIndex >= PAK_MAX_LOADED_ASSETS)
            return false;
        const PakAssetShort_s& asset = globals->loadedAssets[assetIndex];
        if (asset.guid != guid || asset.trackerIndex >= PAK_MAX_TRACKED_ASSETS)
            return false;
        const int32_t owner = globals->trackedAssets[asset.trackerIndex].ownerPakIndex;
        if (owner < 0 || owner >= PAK_MAX_LOADED_PAKS)
            return false;
        const PakLoadedInfo_s& pak = globals->loadedPaks[owner];
        if (!pak.filename || pak.handle == PAK_INVALID_HANDLE)
            return false;
        if (pakPath && *pakPath)
        {
            const auto expected = std::filesystem::absolute(pakPath).lexically_normal();
            const auto actual = std::filesystem::absolute(pak.filename).lexically_normal();
            if (_wcsicmp(expected.c_str(), actual.c_str()) != 0)
                return false;
        }
        handle = atlasHandle;
        return true;
    }
    return false;
}

void RuiReplacePakImageAtlas(void* boundAsset, const void* newHeader, const void* previousHeader)
{
    auto* destination = static_cast<RuiImageAtlas*>(boundAsset);
    const uintptr_t storageAddress = reinterpret_cast<uintptr_t>(g_PakRuiImageAtlases.data());
    const uintptr_t destinationAddress = reinterpret_cast<uintptr_t>(destination);
    const uintptr_t storageOffset = destinationAddress - storageAddress;
    if (destinationAddress < storageAddress || storageOffset >= sizeof(g_PakRuiImageAtlases) || storageOffset % sizeof(RuiImageAtlas) != 0)
    {
        return;
    }

    const size_t atlasIndex = storageOffset / sizeof(RuiImageAtlas);
    const RuiImageAtlasHandle atlasHandle = static_cast<RuiImageAtlasHandle>(atlasIndex);
    const auto* replacement = static_cast<const RuiImageAtlas*>(newHeader);
    const auto* previous = static_cast<const RuiImageAtlas*>(previousHeader);

    std::unique_lock lock(g_RuiImageAtlasMutex);
    RuiBeforeImageAtlasReplace(atlasHandle);
    AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
    if (previous)
        RuiRemoveImageAtlasDescriptors(*previous, atlasHandle);

    *destination = replacement ? *replacement : RuiImageAtlas{};
    if (atlasIndex < RUI_NATIVE_IMAGE_ATLAS_CAPACITY)
        g_RuiImageAtlases[atlasIndex] = *destination;

    if (replacement)
        RuiPublishImageAtlasDescriptors(*destination, atlasHandle, true);
    ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
    RuiAfterImageAtlasReplace(atlasHandle);
}

DECLARE_HOOK(Pak_RegisterAssetBindingType, rtech_game.DLL + 0x7BE0,
             [](auto& hook, PakAssetBinding_s* binding, JobPriority_e priority, uint32_t affinity) -> JobTypeID_t
{
    if ((memcmp(binding->type, "uimg", sizeof(binding->type)) == 0) && binding->version == 10)
    {
        std::copy_n(g_RuiImageAtlases, RUI_NATIVE_IMAGE_ATLAS_CAPACITY, g_PakRuiImageAtlases.begin());
        binding->assetStorage = g_PakRuiImageAtlases.data();
        binding->assetCapacity = static_cast<uint32_t>(g_PakRuiImageAtlases.size());
        binding->replaceAssetFunc = RuiReplacePakImageAtlas;
    }
    RuiConfigureImageAtlasAssetBinding(*binding);

    return hook.Original(binding, priority, affinity);
})

ON_DLL_LOAD_CLIENT("engine.dll", RuiImageAtlasRegistry, [](CModule module)
{
    g_RuiImageAtlases = module.Offset(0x12A26140).RCast<RuiImageAtlas*>();
    g_RuiImageDescriptorMap = module.Offset(0x12A4E508).RCast<RHashMapU32*>();
    RuiImageAtlas_CreateGpuBuffer = module.Offset(0xFBF60).RCast<RuiCreateImageAtlasGpuBuffer_t>();
    RuiImageAtlas_DestroyGpuBuffer = module.Offset(0xFC4F0).RCast<RuiDestroyImageAtlasGpuBuffer_t>();
    g_RuiImageAtlasHandles.fill(RUI_INVALID_IMAGE_ATLAS);
    DISPATCH_MODULE(RuiImageAtlasBindingHooks);
})
