#include "modsystem/modatlas.h"

#include "materialsystem/cmatqueuedrendercontext.h"
#include "rendersystem/schema/texture.g.h"
#include "rtech/pakasset.h"
#include "rtech/paktools.h"
#include "tier0/frametask.h"
#include "tier0/module.h"
#include "windows/id3dx.h"

#include <DirectXTex.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

using Microsoft::WRL::ComPtr;

DECLARE_MODULE(ModAtlasLoadHooks)

struct RuiAtlasPixels
{
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;
    size_t rowPitch;
    std::vector<uint8_t> bytes;
};

struct RuiAtlasTexture
{
    TextureAsset_s original;
    std::shared_ptr<const RuiAtlasPixels> pixels;
};

struct RuiAtlasContents
{
    RuiImageAtlas original;
    uint64_t generation;
    std::vector<RuiImageAtlasGpuRecord> records;
};

struct RuiAppendedImageData
{
    RuiImageAtlasHandle handle;
    RuiImageAtlas original;
    std::vector<RuiImageAtlasEntry> images;
    std::vector<RuiImageDimensions> dimensions;
    std::vector<RuiImageAtlasNineSlice> nineSlices;
    std::vector<RuiImageAtlasNameRecord> names;
    std::string strings;
    uint32_t gpuBuffer = UINT32_MAX;
    bool publishNames = false;
};

struct RuiAtlasAppendSource
{
    RuiImageAtlasHandle handle;
    uint64_t generation;
    const RuiAtlasPixels* pixels;
    bool operator==(const RuiAtlasAppendSource&) const = default;
};

struct RuiAtlasAppendStorage
{
    TextureAsset_s* texture = nullptr;
    TextureAsset_s originalTexture{};
    std::vector<RuiAtlasAppendSource> sources;
    std::vector<std::shared_ptr<const RuiAtlasPixels>> pixels;
    std::vector<RuiAppendedImageData> atlases;
    ComPtr<ID3D11Texture2D> resource;
    ComPtr<ID3D11ShaderResourceView> view;

    ~RuiAtlasAppendStorage()
    {
        for (const auto& atlas : atlases)
        {
            if (atlas.gpuBuffer != UINT32_MAX)
            {
                RuiImageAtlas released = atlas.original;
                released.gpuRecordBuffer = atlas.gpuBuffer;
                RuiImageAtlas_DestroyGpuBuffer(&released);
            }
        }
    }
};

struct RuiAtlasAppendPlan
{
    TextureAsset_s* texture;
    std::vector<RuiImageAtlasHandle> targets;
    std::unordered_map<RuiImageAtlasHandle, std::vector<RuiImageAtlasHandle>> additions;
    std::vector<RuiAtlasAppendSource> sources;
};

std::mutex g_ModAtlasMutex;
std::vector<RuiImageAtlasAppend> g_ModAtlasAppends;
std::unordered_map<const void*, std::shared_ptr<const RuiAtlasPixels>> g_ModAtlasPixelHeaders;
std::unordered_map<TextureAsset_s*, RuiAtlasTexture> g_ModAtlasTextures;
std::unordered_map<RuiImageAtlasHandle, RuiAtlasContents> g_ModAtlasContents;
std::vector<std::shared_ptr<RuiAtlasAppendStorage>> g_ModAtlasStorage;
std::unordered_map<uint16_t, std::vector<std::shared_ptr<RuiAtlasAppendStorage>>> g_ModAtlasFrames;
const DXGI_FORMAT* g_ModAtlasTextureFormats;
PakAssetReplaceFn_t g_ModAtlasReplaceTexture;
PakAssetUnloadFn_t g_ModAtlasUnloadTexture;
bool g_ModAtlasRefreshPending = false;

void RuiRebuildAtlasAppends();

void RuiScheduleAtlasAppends()
{
    {
        std::scoped_lock lock(g_ModAtlasMutex);
        if (g_ModAtlasRefreshPending)
            return;
        g_ModAtlasRefreshPending = true;
    }

	g_TaskQueue.Dispatch([] { RunInRenderThread(RuiRebuildAtlasAppends); });
}

static void RuiRestoreAtlasAppend(const std::shared_ptr<RuiAtlasAppendStorage>& storage)
{
    if (!storage->texture)
        return;
    *storage->texture = storage->originalTexture;

	for (const auto& source : storage->sources)
    {
        if (std::any_of(storage->atlases.begin(), storage->atlases.end(), [&source](const auto& atlas) { return atlas.handle == source.handle; }))
            continue;
        const auto* current = RuiGetImageAtlas(source.handle);
        if (current)
            RuiCommitImageAtlas(source.handle, *current);
    }
    for (const auto& atlas : storage->atlases)
    {
        if (RuiGetImageAtlas(atlas.handle))
            RuiCommitImageAtlas(atlas.handle, atlas.original, atlas.publishNames);
    }
}

static void RuiRestoreAppendsUsing(RuiImageAtlasHandle handle, TextureAsset_s* texture = nullptr)
{
    for (size_t index = g_ModAtlasStorage.size(); index-- != 0;)
    {
        const auto& storage = g_ModAtlasStorage[index];
        const bool affected =
            storage->texture == texture || std::any_of(storage->sources.begin(), storage->sources.end(), [handle, texture](const auto& source)
        {
            const auto found = g_ModAtlasContents.find(source.handle);
            return source.handle == handle || (texture && found != g_ModAtlasContents.end() && found->second.original.texture == texture);
        });
        if (affected)
        {
            RuiRestoreAtlasAppend(storage);
            g_ModAtlasStorage.erase(g_ModAtlasStorage.begin() + index);
        }
    }
}

void RuiBeforeImageAtlasReplace(RuiImageAtlasHandle handle)
{
    std::scoped_lock lock(g_ModAtlasMutex);
    RuiRestoreAppendsUsing(handle);
    g_ModAtlasContents.erase(handle);
}

void RuiAfterImageAtlasReplace(RuiImageAtlasHandle handle)
{
    {
        std::scoped_lock lock(g_ModAtlasMutex);
        const RuiImageAtlas* atlas = RuiGetImageAtlas(handle);
        if (atlas)
        {
            RuiAtlasContents contents{*atlas, RuiGetImageAtlasGpuGeneration(*atlas), {}};
            contents.records.resize(atlas->imageCount);
            for (uint16_t index = 0; index < atlas->imageCount; ++index)
            {
                if (!RuiGetImageAtlasGpuRecord(*atlas, index, contents.records[index]))
                    return;
            }
            g_ModAtlasContents.insert_or_assign(handle, std::move(contents));
        }
    }
    RuiScheduleAtlasAppends();
}

void RuiCaptureAtlasPixels(PakFile& pak, const RPakAssetEntryV7_s& asset)
{
    if (!g_ModAtlasTextureFormats || asset.assetType != 0x72747874 || asset.version != 8 || asset.cpuPtr.pageIndex == UINT32_MAX)
        return;
    bool isAtlasTexture = false;
    for (uint32_t index = 0; index < asset.dependentsCount; ++index)
    {
        const uint32_t dependent = pak.sections.assetDependents[asset.dependentsIndex + index];
        if (dependent < pak.header.assetCount && pak.sections.assetEntries[dependent].assetType == 0x676D6975)
        {
            isAtlasTexture = true;
            break;
        }
    }
    if (!isAtlasTexture)
        return;
    auto* header = static_cast<TextureAsset_s*>(pak.GetPointerForPageOffset(&asset.headerPtr));
    if (header->depth || header->arraySize > 1 || header->layerCount > 1 || !header->permanentMipLevels || !header->width || !header->height ||
        header->imageFormat >= 64 || header->streamedMipLevels + header->permanentMipLevels > 16)
    {
        spdlog::error("Atlas texture '{}' has an unsupported native texture layout", header->debugName ? header->debugName : "");
        return;
    }
    const DXGI_FORMAT format = g_ModAtlasTextureFormats[header->imageFormat];
    size_t offset = 0;
    size_t rowPitch = 0;
    size_t slicePitch = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    for (uint32_t mip = header->streamedMipLevels + header->permanentMipLevels; mip-- > header->streamedMipLevels;)
    {
        width = std::max(1u, static_cast<uint32_t>(header->width) >> mip);
        height = std::max(1u, static_cast<uint32_t>(header->height) >> mip);
        if (FAILED(DirectX::ComputePitch(format, width, height, rowPitch, slicePitch)))
            return;
        if (mip == header->streamedMipLevels)
            break;
        offset += (slicePitch + 15) & ~size_t(15);
    }
    if (!pak.IsPageOffsetValid(asset.cpuPtr.pageIndex, asset.cpuPtr.offset) ||
        offset + slicePitch > static_cast<size_t>(pak.sections.pageHeaders[asset.cpuPtr.pageIndex].dataSize) - asset.cpuPtr.offset)
    {
        spdlog::error("Atlas texture '{}' has a truncated resident mip", header->debugName ? header->debugName : "");
        return;
    }
    const auto* data = static_cast<const uint8_t*>(pak.GetPointerForPageOffset(&asset.cpuPtr)) + offset;
    auto pixels = std::make_shared<RuiAtlasPixels>();
    pixels->width = width;
    pixels->height = height;
    pixels->format = format;
    pixels->rowPitch = rowPitch;
    pixels->bytes.assign(data, data + slicePitch);
    std::scoped_lock lock(g_ModAtlasMutex);
    g_ModAtlasPixelHeaders.insert_or_assign(header, std::move(pixels));
}

DECLARE_HOOK(ModAtlasDispatchLoad, rtech_game.dll + 0x85B0, [](auto& hook, uint64_t pakHandle, uint64_t assetIndex) -> void
{
    PakGlobalState_s* globals = Pak_GetGlobals();
    PakFile* pak = globals ? globals->loadedPaks[pakHandle & PAK_MAX_LOADED_PAKS_MASK].pakFile : nullptr;
    if (pak && assetIndex < pak->header.assetCount)
        RuiCaptureAtlasPixels(*pak, pak->sections.assetEntries[assetIndex]);
    hook.Original(pakHandle, assetIndex);
})

void RuiReplaceAtlasTexture(void* boundAsset, const void* newHeader, const void* previousHeader)
{
    bool tracked;
    {
        std::scoped_lock lock(g_ModAtlasMutex);
        tracked = g_ModAtlasPixelHeaders.contains(newHeader) || g_ModAtlasTextures.contains(static_cast<TextureAsset_s*>(boundAsset));
    }
    if (!tracked)
    {
        g_ModAtlasReplaceTexture(boundAsset, newHeader, previousHeader);
        return;
    }
    {
        std::unique_lock registryLock(g_RuiImageAtlasMutex);
        std::scoped_lock lock(g_ModAtlasMutex);
        auto* texture = static_cast<TextureAsset_s*>(boundAsset);
        RuiRestoreAppendsUsing(RUI_INVALID_IMAGE_ATLAS, texture);
        g_ModAtlasReplaceTexture(boundAsset, newHeader, previousHeader);
        g_ModAtlasTextures.erase(texture);
        const auto found = g_ModAtlasPixelHeaders.find(newHeader);
        if (newHeader && found != g_ModAtlasPixelHeaders.end())
            g_ModAtlasTextures.emplace(texture, RuiAtlasTexture{*texture, found->second});
    }
    RuiScheduleAtlasAppends();
}

void RuiUnloadAtlasTexture(void* header)
{
    {
        std::scoped_lock lock(g_ModAtlasMutex);
        g_ModAtlasPixelHeaders.erase(header);
    }
    g_ModAtlasUnloadTexture(header);
}

void RuiConfigureImageAtlasAssetBinding(PakAssetBinding_s& binding)
{
    if (memcmp(binding.type, "txtr", sizeof(binding.type)) != 0 || binding.version != 8)
        return;
    g_ModAtlasReplaceTexture = binding.replaceAssetFunc;
    g_ModAtlasUnloadTexture = binding.unloadAssetFunc;
    binding.replaceAssetFunc = RuiReplaceAtlasTexture;
    binding.unloadAssetFunc = RuiUnloadAtlasTexture;
}

bool RuiDecodeAtlasPixels(const RuiAtlasPixels& pixels, DirectX::ScratchImage& converted, DirectX::Image& image)
{
    image = {pixels.width, pixels.height, pixels.format, pixels.rowPitch, pixels.bytes.size(), const_cast<uint8_t*>(pixels.bytes.data())};
    if (image.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        return true;
    DirectX::ScratchImage decompressed;
    if (DirectX::IsCompressed(image.format))
    {
        if (FAILED(DirectX::Decompress(image, DXGI_FORMAT_UNKNOWN, decompressed)))
            return false;
        image = *decompressed.GetImage(0, 0, 0);
    }
    if (image.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        converted = std::move(decompressed);
    else if (FAILED(DirectX::Convert(image, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DirectX::TEX_FILTER_DEFAULT, 0.5f, converted)))
        return false;
    image = *converted.GetImage(0, 0, 0);
    return true;
}

struct RuiAtlasPlacement
{
    const RuiAtlasPixels* pixels;
    uint32_t x;
    uint32_t y;
};

bool RuiPlaceAtlasTextures(std::vector<RuiAtlasPlacement>& placements, uint32_t& width, uint32_t& height)
{
    constexpr uint32_t limit = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    uint32_t minimum = 0;
    for (const auto& placement : placements)
        minimum = std::max(minimum, placement.pixels->width);
    if (minimum > limit || placements.front().pixels->height > limit)
        return false;
    for (uint32_t candidate = minimum;; candidate = std::min(limit, candidate * 2))
    {
        uint32_t x = 0;
        uint32_t y = placements.front().pixels->height + 2;
        uint32_t shelfHeight = 0;
        width = placements.front().pixels->width;
        height = placements.front().pixels->height;
        placements.front().x = placements.front().y = 0;
        for (size_t index = 1; index < placements.size(); ++index)
        {
            auto& placement = placements[index];
            if (x + placement.pixels->width > candidate)
            {
                y += shelfHeight + 2;
                x = shelfHeight = 0;
            }
            placement.x = x;
            placement.y = y;
            width = std::max(width, x + placement.pixels->width);
            height = std::max(height, y + placement.pixels->height);
            shelfHeight = std::max(shelfHeight, placement.pixels->height);
            x += placement.pixels->width + 2;
        }
        if (height <= limit)
            return true;
        if (candidate == limit)
            return false;
    }
}

bool RuiBuildAtlasAppend(const RuiAtlasAppendPlan& plan, RuiAtlasAppendStorage& storage, ID3D11Device* device)
{
    const auto texture = g_ModAtlasTextures.find(plan.texture);
    if (texture == g_ModAtlasTextures.end())
        return false;
    storage.texture = plan.texture;
    storage.originalTexture = texture->second.original;
    if (storage.originalTexture.streamedMipLevels)
    {
        spdlog::error("Cannot append to streaming atlas texture '{}'", storage.originalTexture.debugName);
        return false;
    }
    storage.sources = plan.sources;
    std::vector<RuiAtlasPlacement> placements{{texture->second.pixels.get(), 0, 0}};
    storage.pixels.push_back(texture->second.pixels);
    for (const auto& source : plan.sources)
    {
        if (std::none_of(placements.begin(), placements.end(), [&source](const auto& placement) { return placement.pixels == source.pixels; }))
        {
            placements.push_back({source.pixels, 0, 0});
            const auto& contents = g_ModAtlasContents.at(source.handle);
            storage.pixels.push_back(g_ModAtlasTextures.at(static_cast<TextureAsset_s*>(contents.original.texture)).pixels);
        }
    }
    uint32_t width;
    uint32_t height;
    if (!RuiPlaceAtlasTextures(placements, width, height))
        return false;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    for (const auto& placement : placements)
    {
        DirectX::ScratchImage decoded;
        DirectX::Image source;
        if (!RuiDecodeAtlasPixels(*placement.pixels, decoded, source))
            return false;
        for (size_t row = 0; row < source.height; ++row)
            memcpy(pixels.data() + ((row + placement.y) * width + placement.x) * 4, source.pixels + row * source.rowPitch, source.width * 4);
    }
    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA initial{pixels.data(), width * 4, width * height * 4};
    if (FAILED(device->CreateTexture2D(&description, &initial, &storage.resource)) ||
        FAILED(device->CreateShaderResourceView(storage.resource.Get(), nullptr, &storage.view)))
        return false;

    storage.atlases.reserve(plan.targets.size());
    for (RuiImageAtlasHandle target : plan.targets)
    {
        const auto& base = g_ModAtlasContents.at(target);
        auto& result = storage.atlases.emplace_back();
        result.handle = target;
        result.original = base.original;
        result.publishNames = plan.additions.contains(target);
        std::vector<RuiImageAtlasHandle> contributors{target};
        if (const auto found = plan.additions.find(target); found != plan.additions.end())
            contributors.insert(contributors.end(), found->second.begin(), found->second.end());
        struct Entry
        {
            RuiImageAtlasHandle handle;
            uint16_t index;
        };
        std::vector<Entry> entries;
        for (RuiImageAtlasHandle contributor : contributors)
        {
            const auto& atlas = g_ModAtlasContents.at(contributor).original;
            if (!atlas.images || !atlas.imageDimensions || !atlas.imageNameRecords || !atlas.imageNames ||
                atlas.nineSliceImageCount > atlas.imageCount || (atlas.nineSliceImageCount && !atlas.nineSliceData))
                return false;
            for (uint32_t index = 0; index < atlas.imageCount; ++index)
                entries.push_back({contributor, static_cast<uint16_t>(index)});
        }
        if (entries.size() > INT16_MAX)
            return false;
        std::stable_partition(entries.begin(), entries.end(),
                              [](const Entry& entry) { return entry.index < g_ModAtlasContents.at(entry.handle).original.nineSliceImageCount; });
        std::unordered_set<uint32_t> names;
        std::vector<RuiImageAtlasGpuRecord> records;
        records.reserve(entries.size());
        for (const Entry& entry : entries)
        {
            const auto& source = g_ModAtlasContents.at(entry.handle);
            const auto& name = source.original.imageNameRecords[entry.index];
            if (!names.insert(name.nameHash).second)
            {
                spdlog::error("Atlas append would duplicate image '{}' in target {}", source.original.imageNames + name.nameOffset, target);
                return false;
            }
            const char* text = source.original.imageNames + name.nameOffset;
            const size_t length = strlen(text) + 1;
            if (result.strings.size() + length > UINT16_MAX + size_t(1))
                return false;
            result.names.push_back({name.nameHash, name.flags, static_cast<uint16_t>(result.strings.size())});
            result.strings.append(text, length);
            result.images.push_back(source.original.images[entry.index]);
            result.dimensions.push_back(source.original.imageDimensions[entry.index]);
            if (entry.index < source.original.nineSliceImageCount)
                result.nineSlices.push_back(source.original.nineSliceData[entry.index]);
            const auto* sourcePixels = g_ModAtlasTextures.at(static_cast<TextureAsset_s*>(source.original.texture)).pixels.get();
            const auto placement =
                std::find_if(placements.begin(), placements.end(), [sourcePixels](const auto& value) { return value.pixels == sourcePixels; });
            const auto& uv = source.records[entry.index];
            records.push_back(
                {{(placement->x + uv.uvMin.x * sourcePixels->width) / width, (placement->y + uv.uvMin.y * sourcePixels->height) / height},
                 {uv.uvSize.x * sourcePixels->width / width, uv.uvSize.y * sourcePixels->height / height}});
        }
        RuiImageAtlas contents = base.original;
        contents.inverseWidth = 1.0f / width;
        contents.inverseHeight = 1.0f / height;
        contents.width = static_cast<uint16_t>(width);
        contents.height = static_cast<uint16_t>(height);
        contents.imageCount = static_cast<uint16_t>(result.images.size());
        contents.nineSliceImageCount = static_cast<uint16_t>(result.nineSlices.size());
        contents.images = result.images.data();
        contents.imageDimensions = result.dimensions.data();
        contents.nineSliceData = result.nineSlices.data();
        contents.imageNameRecords = result.names.data();
        contents.imageNames = result.strings.data();
        contents.gpuRecordBuffer = UINT32_MAX;
        result.gpuBuffer = RuiImageAtlas_CreateGpuBuffer(&contents, records.data());
        if (result.gpuBuffer == UINT32_MAX || !RuiCommitImageAtlas(target, contents, result.publishNames))
            return false;
    }
    TextureAsset_s& targetTexture = *storage.texture;
    targetTexture.width = static_cast<uint16_t>(width);
    targetTexture.height = static_cast<uint16_t>(height);
    targetTexture.imageFormat = 32;
    targetTexture.dataSize = width * height * 4;
    targetTexture.permanentMipLevels = 1;
    targetTexture.texelCount = static_cast<uint64_t>(width) * height;
    targetTexture.d3d11Resource = storage.resource.Get();
    targetTexture.shaderResourceView = storage.view.Get();
    targetTexture.unknownByte128 = 1;
    return true;
}

void RuiRebuildAtlasAppends()
{
    std::unique_lock registryLock(g_RuiImageAtlasMutex);
    std::scoped_lock lock(g_ModAtlasMutex);
    g_ModAtlasRefreshPending = false;
    ID3D11Device* const device = D3D11Device();
    if (!device || !RuiImageAtlas_CreateGpuBuffer)
        return;
    std::unordered_map<RuiImageAtlasHandle, std::vector<RuiImageAtlasHandle>> additions;
    for (const auto& declaration : g_ModAtlasAppends)
    {
        RuiImageAtlasHandle source;
        RuiImageAtlasHandle target;
        if (!RuiResolvePakImageAtlas(declaration.pakPath.c_str(), declaration.sourceAtlasPath.c_str(), source) ||
            !RuiResolvePakImageAtlas(nullptr, declaration.targetAtlasPath.c_str(), target))
            continue;
        if (source == target)
        {
            spdlog::error("Atlas '{}' cannot append to itself", declaration.sourceAtlasPath);
            continue;
        }
        auto& sources = additions[target];
        if (std::find(sources.begin(), sources.end(), source) == sources.end())
            sources.push_back(source);
    }
    std::vector<RuiAtlasAppendPlan> plans;
    for (const auto& [target, directSources] : additions)
    {
        const auto targetContents = g_ModAtlasContents.find(target);
        if (targetContents == g_ModAtlasContents.end())
            continue;
        auto* texture = static_cast<TextureAsset_s*>(targetContents->second.original.texture);
        if (!g_ModAtlasTextures.contains(texture))
            continue;
        std::vector<RuiImageAtlasHandle> sources;
        std::unordered_set<RuiImageAtlasHandle> visiting{target};
        std::unordered_set<RuiImageAtlasHandle> included;
        const auto expand = [&](auto&& self, RuiImageAtlasHandle source) -> bool
        {
            if (visiting.contains(source))
                return false;
            if (!included.insert(source).second)
                return true;
            const auto contents = g_ModAtlasContents.find(source);
            if (contents == g_ModAtlasContents.end() || !g_ModAtlasTextures.contains(static_cast<TextureAsset_s*>(contents->second.original.texture)))
                return false;
            visiting.insert(source);
            if (const auto dependencies = additions.find(source); dependencies != additions.end())
                for (RuiImageAtlasHandle dependency : dependencies->second)
                    if (!self(self, dependency))
                        return false;
            visiting.erase(source);
            sources.push_back(source);
            return true;
        };
        bool valid = true;
        for (RuiImageAtlasHandle source : directSources)
            valid = expand(expand, source) && valid;
        if (!valid)
        {
            spdlog::error("Atlas append target {} has a cycle or unavailable CPU texture data", target);
            continue;
        }
        auto found = std::find_if(plans.begin(), plans.end(), [texture](const auto& plan) { return plan.texture == texture; });
        if (found == plans.end())
        {
            plans.push_back({texture});
            found = std::prev(plans.end());
            for (const auto& [handle, contents] : g_ModAtlasContents)
                if (contents.original.texture == texture)
                    found->targets.push_back(handle);
            std::sort(found->targets.begin(), found->targets.end());
        }
        found->additions.emplace(target, std::move(sources));
    }
    for (auto& plan : plans)
    {
        std::vector<RuiImageAtlasHandle> sources = plan.targets;
        for (const auto& [target, additions] : plan.additions)
            sources.insert(sources.end(), additions.begin(), additions.end());
        std::sort(sources.begin(), sources.end());
        sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
        for (RuiImageAtlasHandle source : sources)
        {
            const auto& contents = g_ModAtlasContents.at(source);
            plan.sources.push_back(
                {source, contents.generation, g_ModAtlasTextures.at(static_cast<TextureAsset_s*>(contents.original.texture)).pixels.get()});
        }
    }
    for (size_t index = g_ModAtlasStorage.size(); index-- != 0;)
    {
        const auto& storage = g_ModAtlasStorage[index];
        const auto found = std::find_if(plans.begin(), plans.end(), [&storage](const auto& plan)
        { return plan.texture == storage->texture && plan.sources == storage->sources; });
        if (found == plans.end())
        {
            RuiRestoreAtlasAppend(storage);
            g_ModAtlasStorage.erase(g_ModAtlasStorage.begin() + index);
        }
    }

	std::vector<size_t> order;
    std::vector<bool> pending(plans.size(), true);
    while (order.size() < plans.size())
    {
        const size_t previousCount = order.size();
        for (size_t index = 0; index < plans.size(); ++index)
        {
            if (!pending[index])
                continue;
            bool waiting = false;
            for (size_t dependency = 0; dependency < plans.size() && !waiting; ++dependency)
            {
                if (dependency == index || !pending[dependency])
                    continue;
                for (const auto& [target, sources] : plans[index].additions)
                    if (std::any_of(sources.begin(), sources.end(),
                                    [&](RuiImageAtlasHandle source) { return plans[dependency].additions.contains(source); }))
                        waiting = true;
            }
            if (!waiting)
            {
                pending[index] = false;
                order.push_back(index);
            }
        }
        if (order.size() == previousCount)
        {
            spdlog::error("Atlas append targets form a cyclic texture dependency");
            break;
        }
    }
    for (size_t index : order)
    {
        const auto& plan = plans[index];
        if (std::any_of(g_ModAtlasStorage.begin(), g_ModAtlasStorage.end(),
                        [&plan](const auto& storage) { return storage->texture == plan.texture; }))
            continue;
        auto storage = std::make_shared<RuiAtlasAppendStorage>();
        if (!RuiBuildAtlasAppend(plan, *storage, device))
        {
            RuiRestoreAtlasAppend(storage);
            spdlog::error("Failed to append image atlases to existing texture '{}'", storage->originalTexture.debugName);
            continue;
        }
        for (auto& [stage, retained] : g_ModAtlasFrames)
            retained.push_back(storage);
        g_ModAtlasStorage.push_back(std::move(storage));
    }
}

void RuiConfigureImageAtlasAppends(std::span<const RuiImageAtlasAppend> appends)
{
    {
        std::unique_lock registryLock(g_RuiImageAtlasMutex);
        std::scoped_lock lock(g_ModAtlasMutex);
        for (size_t index = g_ModAtlasStorage.size(); index-- != 0;)
            RuiRestoreAtlasAppend(g_ModAtlasStorage[index]);
        g_ModAtlasStorage.clear();
        g_ModAtlasAppends.assign(appends.begin(), appends.end());
    }
    RuiScheduleAtlasAppends();
}

void RuiBeginImageAtlasFrame(uint16_t rendererIndex)
{
    std::scoped_lock lock(g_ModAtlasMutex);
    g_ModAtlasFrames[rendererIndex] = g_ModAtlasStorage;
}

ON_DLL_LOAD_CLIENT("rtech_game.dll", ModAtlasLoad, [](CModule module) { DISPATCH_MODULE(ModAtlasLoadHooks); })
ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", ModAtlasFormats,
                   [](CModule module) { g_ModAtlasTextureFormats = module.Offset(0x1C3280).RCast<const DXGI_FORMAT*>(); })
