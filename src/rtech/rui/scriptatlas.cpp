#include "rtech/rui/scriptatlas.h"

#include "config/profile.h"
#include "dedicated/dedicated.h"
#include "materialsystem/cmatqueuedrendercontext.h"
#include "modsystem/modmanager.h"
#include "rendersystem/schema/texture.g.h"
#include "rtech/paktools.h"
#include "rtech/rstdlib.h"
#include "rtech/rui/atlas.h"
#include "tier0/frametask.h"
#include "vscript/scripts/scripthttprequesthandler.h"
#include "windows/id3dx.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <curl/curl.h>
#include <deque>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <webp/decode.h>
#include <wincodec.h>
#include <wrl/client.h>

extern RHashMapU32* g_RuiImageDescriptorMap;

static constexpr size_t SCRIPT_ATLAS_MAX_ENCODED = 16 * 1024 * 1024;
static constexpr uint32_t SCRIPT_ATLAS_MAX_DIMENSION = 8192;
static constexpr uint64_t SCRIPT_ATLAS_MAX_PIXELS = 64ull * 1024 * 1024;
static constexpr size_t SCRIPT_ATLAS_MEMORY_BUDGET = 256 * 1024 * 1024;
static constexpr size_t SCRIPT_ATLAS_MAX_IMAGES = 2048;
static constexpr size_t SCRIPT_ATLAS_MAX_ATLASES = 64;

struct ScriptAtlasFile
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~ScriptAtlasFile()
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};

bool ScriptAtlasReadFile(const fs::path& path, const fs::path& root, std::vector<uint8_t>& bytes)
{
    ScriptAtlasFile file;
    file.handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file.handle == INVALID_HANDLE_VALUE)
        return false;
    std::wstring finalPath(32768, L'\0');
    const DWORD length = GetFinalPathNameByHandleW(file.handle, finalPath.data(), static_cast<DWORD>(finalPath.size()), FILE_NAME_NORMALIZED);
    if (!length || length >= finalPath.size())
        return false;
    finalPath.resize(length);
    if (finalPath.starts_with(L"\\\\?\\UNC\\"))
        finalPath = L"\\\\" + finalPath.substr(8);
    else if (finalPath.starts_with(L"\\\\?\\"))
        finalPath.erase(0, 4);
    if (!ModPaths::IsAtOrBelow(fs::path(finalPath), root))
        return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.handle, &size) || size.QuadPart <= 0 || size.QuadPart > SCRIPT_ATLAS_MAX_ENCODED)
        return false;
    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    return ReadFile(file.handle, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
}

bool ScriptAtlasResolveLocal(const std::string& source, fs::path& path, fs::path& root)
{
    if (!g_pModManager || source.empty() || source.find('\0') != std::string::npos)
        return false;
    const fs::path requested = fs::u8path(source);
    if (requested.is_absolute())
    {
        if (source.size() < 3 || source[1] != ':' || source.find(':', 2) != std::string::npos ||
            !((source[0] >= 'A' && source[0] <= 'Z') || (source[0] >= 'a' && source[0] <= 'z')))
            return false;
        std::error_code error;
        path = fs::canonical(requested, error);
        if (error)
            return false;
        for (const Mod& mod : g_pModManager->m_LoadedMods)
        {
            std::array<fs::path, 3> roots{mod.m_ModDirectory, mod.m_PackageDirectory, {}};
            if (mod.IsRemote())
            {
                const fs::path remote = GetRemoteModFolderPath().lexically_normal();
                const fs::path relative = mod.m_ModDirectory.lexically_normal().lexically_relative(remote);
                if (!relative.empty() && *relative.begin() != ".." && *relative.begin() != ".")
                    roots[2] = remote / *relative.begin();
            }
            for (const fs::path& candidate : roots)
            {
                if (candidate.empty())
                    continue;
                const fs::path canonical = fs::canonical(candidate, error);
                if (!error && ModPaths::IsAtOrBelow(path, canonical))
                {
                    root = canonical;
                    return true;
                }
                error.clear();
            }
        }
        return false;
    }
    if (requested.has_root_path() || source.find(':') != std::string::npos)
        return false;
    for (const fs::path& component : requested)
        if (component == ".." || component == ".")
            return false;
    const auto found = g_pModManager->m_ModFiles.find(g_pModManager->NormaliseModFilePath(requested));
    if (found == g_pModManager->m_ModFiles.end() || !found->second.m_pOwningMod || !found->second.m_pOwningMod->m_bEnabled)
        return false;
    std::error_code error;
    root = fs::canonical(found->second.m_pOwningMod->m_ModDirectory / MOD_OVERRIDE_DIR, error);
    if (error)
        return false;
    path = fs::canonical(root / requested, error);
    return !error && ModPaths::IsAtOrBelow(path, root);
}

struct ScriptAtlasFitRect
{
    uint32_t cropX = 0, cropY = 0, cropWidth, cropHeight;
    uint32_t outputX = 0, outputY = 0, outputWidth, outputHeight;
};

ScriptAtlasFitRect ScriptAtlasComputeFit(uint32_t width, uint32_t height, const ScriptAtlasImage& image, ScriptAtlasFit fit)
{
    ScriptAtlasFitRect rect{0, 0, width, height, 0, 0, image.width, image.height};
    const uint64_t sourceAspect = static_cast<uint64_t>(width) * image.height;
    const uint64_t targetAspect = static_cast<uint64_t>(height) * image.width;
    if (fit == ScriptAtlasFit::Cover)
    {
        if (sourceAspect > targetAspect)
            rect.cropWidth = std::max(1u, static_cast<uint32_t>(static_cast<uint64_t>(height) * image.width / image.height));
        else if (sourceAspect < targetAspect)
            rect.cropHeight = std::max(1u, static_cast<uint32_t>(static_cast<uint64_t>(width) * image.height / image.width));
        rect.cropX = (width - rect.cropWidth) / 2;
        rect.cropY = (height - rect.cropHeight) / 2;
    }
    else if (fit == ScriptAtlasFit::Contain)
    {
        if (sourceAspect > targetAspect)
            rect.outputHeight = std::max(1u, static_cast<uint32_t>(static_cast<uint64_t>(image.width) * height / width));
        else if (sourceAspect < targetAspect)
            rect.outputWidth = std::max(1u, static_cast<uint32_t>(static_cast<uint64_t>(image.height) * width / height));
        rect.outputX = (image.width - rect.outputWidth) / 2;
        rect.outputY = (image.height - rect.outputHeight) / 2;
    }
    return rect;
}

bool ScriptAtlasValidDimensions(uint32_t width, uint32_t height)
{
    return width && height && width <= SCRIPT_ATLAS_MAX_DIMENSION && height <= SCRIPT_ATLAS_MAX_DIMENSION &&
           static_cast<uint64_t>(width) * height <= SCRIPT_ATLAS_MAX_PIXELS;
}

bool ScriptAtlasDecode(std::vector<uint8_t>& encoded, const ScriptAtlasImage& image, ScriptAtlasFit fit, std::vector<uint8_t>& pixels)
{
    const uint32_t paddedWidth = image.width + image.gutter * 2;
    const uint32_t paddedHeight = image.height + image.gutter * 2;
    const uint32_t pitch = paddedWidth * 4;
    pixels.assign(static_cast<size_t>(pitch) * paddedHeight, 0);
    const bool webp = encoded.size() >= 12 && std::memcmp(encoded.data(), "RIFF", 4) == 0 && std::memcmp(encoded.data() + 8, "WEBP", 4) == 0;
    if (webp)
    {
        WebPDecoderConfig config{};
        if (!WebPInitDecoderConfig(&config) || WebPGetFeatures(encoded.data(), encoded.size(), &config.input) != VP8_STATUS_OK ||
            !ScriptAtlasValidDimensions(config.input.width, config.input.height) || config.input.has_animation)
            return false;
        ScriptAtlasFitRect rect = ScriptAtlasComputeFit(config.input.width, config.input.height, image, fit);
        rect.cropX &= ~1u;
        rect.cropY &= ~1u;
        config.options.use_cropping = 1;
        config.options.crop_left = rect.cropX;
        config.options.crop_top = rect.cropY;
        config.options.crop_width = rect.cropWidth;
        config.options.crop_height = rect.cropHeight;
        config.options.use_scaling = 1;
        config.options.scaled_width = rect.outputWidth;
        config.options.scaled_height = rect.outputHeight;
        config.output.colorspace = MODE_RGBA;
        config.output.is_external_memory = 1;
        const size_t offset = static_cast<size_t>(image.gutter + rect.outputY) * pitch + (image.gutter + rect.outputX) * 4;
        config.output.u.RGBA.rgba = pixels.data() + offset;
        config.output.u.RGBA.stride = pitch;
        config.output.u.RGBA.size = pixels.size() - offset;
        const VP8StatusCode status = WebPDecode(encoded.data(), encoded.size(), &config);
        WebPFreeDecBuffer(&config.output);
        if (status != VP8_STATUS_OK)
            return false;
    }
    else
    {
        using Microsoft::WRL::ComPtr;
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
            FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(encoded.data(), static_cast<DWORD>(encoded.size()))) ||
            FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
            return false;
        GUID container{};
        UINT frames = 0, width = 0, height = 0;
        if (FAILED(decoder->GetContainerFormat(&container)) || (container != GUID_ContainerFormatPng && container != GUID_ContainerFormatJpeg) ||
            FAILED(decoder->GetFrameCount(&frames)) || frames != 1 || FAILED(decoder->GetFrame(0, &frame)) ||
            FAILED(frame->GetSize(&width, &height)) || !ScriptAtlasValidDimensions(width, height))
            return false;
        const ScriptAtlasFitRect rect = ScriptAtlasComputeFit(width, height, image, fit);
        WICRect crop{static_cast<INT>(rect.cropX), static_cast<INT>(rect.cropY), static_cast<INT>(rect.cropWidth), static_cast<INT>(rect.cropHeight)};
        ComPtr<IWICBitmapClipper> clipper;
        ComPtr<IWICBitmapScaler> scaler;
        ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateBitmapClipper(&clipper)) || FAILED(clipper->Initialize(frame.Get(), &crop)) ||
            FAILED(factory->CreateBitmapScaler(&scaler)) ||
            FAILED(scaler->Initialize(clipper.Get(), rect.outputWidth, rect.outputHeight, WICBitmapInterpolationModeFant)) ||
            FAILED(factory->CreateFormatConverter(&converter)) ||
            FAILED(
                converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
            return false;
        const size_t offset = static_cast<size_t>(image.gutter + rect.outputY) * pitch + (image.gutter + rect.outputX) * 4;
        if (FAILED(converter->CopyPixels(nullptr, pitch, static_cast<UINT>(pixels.size() - offset), pixels.data() + offset)))
            return false;
    }
    for (uint32_t y = image.gutter; y < image.gutter + image.height; ++y)
    {
        uint8_t* row = pixels.data() + static_cast<size_t>(y) * pitch;
        for (uint32_t x = 0; x < image.gutter; ++x)
        {
            std::memcpy(row + x * 4, row + image.gutter * 4, 4);
            std::memcpy(row + (paddedWidth - x - 1) * 4, row + (image.gutter + image.width - 1) * 4, 4);
        }
    }
    for (uint32_t y = 0; y < image.gutter; ++y)
    {
        std::memcpy(pixels.data() + static_cast<size_t>(y) * pitch, pixels.data() + static_cast<size_t>(image.gutter) * pitch, pitch);
        std::memcpy(pixels.data() + static_cast<size_t>(paddedHeight - y - 1) * pitch,
                    pixels.data() + static_cast<size_t>(image.gutter + image.height - 1) * pitch, pitch);
    }
    return true;
}

struct CScriptAtlasManager::Impl
{
    struct Image
    {
        ScriptAtlasImageState state = ScriptAtlasImageState::Empty;
        std::shared_ptr<std::atomic_bool> cancelled;
        bool dirty = true;
    };
    struct Atlas
    {
        int id;
        uintptr_t owner;
        std::string name;
        uint32_t width, height;
        bool alive = true, queued = false, registrationFailed = false;
        std::vector<ScriptAtlasImage> layout;
        std::vector<Image> images;
        std::vector<uint8_t> pixels;
        std::vector<RuiImageAtlasEntry> entries;
        std::vector<RuiImageDimensions> dimensions;
        std::vector<RuiImageAtlasNameRecord> names;
        std::vector<char> strings;
        std::vector<RuiImageAtlasGpuRecord> records;
        TextureAsset_s asset{};
        RuiImageAtlasHandle handle = RUI_INVALID_IMAGE_ATLAS;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        ID3D11Device* device = nullptr;
    };
    struct Job
    {
        std::shared_ptr<Atlas> atlas;
        size_t image;
        std::shared_ptr<std::atomic_bool> cancelled;
        std::string source, version;
        fs::path path, root;
        ScriptAtlasFit fit;
    };
    std::mutex mutex;
    std::condition_variable changed;
    std::unordered_map<int, std::shared_ptr<Atlas>> atlases;
    std::deque<Job> jobs;
    std::vector<std::thread> workers;
    std::atomic_bool stopped = false;
    uint64_t nextId = 1;
    size_t allocated = 0;
    bool maintenanceQueued = false;
    std::mutex cacheMutex;
    std::atomic_uint64_t temporarySequence = 0;
    std::chrono::steady_clock::time_point lastPrune{};

    std::shared_ptr<Atlas> Find(uintptr_t owner, int id)
    {
        const auto found = atlases.find(id);
        return found != atlases.end() && found->second->alive && found->second->owner == owner ? found->second : nullptr;
    }
    bool Cancelled(const Job& job) const
    {
        return stopped.load() || job.cancelled->load();
    }
    void QueueRender(const std::shared_ptr<Atlas>& atlas);
    void QueueMaintenance();
    void Render(const std::shared_ptr<Atlas>& atlas);
    void Retire(const std::shared_ptr<Atlas>& atlas);
    static void ReleaseGpu(Atlas& atlas);
    void Worker();
    bool Download(const Job& job, std::vector<uint8_t>& bytes);
    bool Encoded(const Job& job, std::vector<uint8_t>& bytes, fs::path& cachePath);
    void Prune(const fs::path& directory);
};

void CScriptAtlasManager::Impl::QueueMaintenance()
{
    if (maintenanceQueued || stopped.load())
        return;
    maintenanceQueued = true;
    g_TaskQueue.Dispatch([this]
    {
        std::scoped_lock lock(mutex);
        maintenanceQueued = false;
        if (stopped.load())
            return;
        bool active = false;
        ID3D11Device* const device = D3D11Device();
        const bool available = device && D3D11DeviceContext() && SUCCEEDED(device->GetDeviceRemovedReason());
        for (const auto& [id, atlas] : atlases)
        {
            if (!atlas->alive)
                continue;
            active = true;
            if (atlas->device != device || (!available && atlas->texture) || (available && !atlas->texture && !atlas->registrationFailed))
                QueueRender(atlas);
        }
        if (active)
            QueueMaintenance();
    }, 30);
}

void CScriptAtlasManager::Impl::ReleaseGpu(Atlas& atlas)
{
    if (atlas.handle != RUI_INVALID_IMAGE_ATLAS)
        RuiUnregisterImageAtlas(atlas.handle);
    atlas.handle = RUI_INVALID_IMAGE_ATLAS;
    atlas.asset = {};
    atlas.view.Reset();
    atlas.texture.Reset();
    atlas.device = nullptr;
}

void CScriptAtlasManager::Impl::QueueRender(const std::shared_ptr<Atlas>& atlas)
{
    if (atlas->queued)
        return;
    atlas->queued = true;
    g_TaskQueue.Dispatch([this, atlas] { RunInRenderThread([this, atlas] { Render(atlas); }); });
}

void CScriptAtlasManager::Impl::Retire(const std::shared_ptr<Atlas>& atlas)
{
    atlas->alive = false;
    for (Image& image : atlas->images)
        if (image.cancelled)
            image.cancelled->store(true);
    std::erase_if(jobs, [&](const Job& job) { return job.atlas == atlas; });
    QueueRender(atlas);
}

void CScriptAtlasManager::Impl::Render(const std::shared_ptr<Atlas>& atlas)
{
    std::scoped_lock lock(mutex);
    atlas->queued = false;
    if (!atlas->alive)
    {
        ReleaseGpu(*atlas);
        allocated -= atlas->pixels.size();
        std::vector<uint8_t>().swap(atlas->pixels);
        atlases.erase(atlas->id);
        return;
    }
    ID3D11Device* const device = D3D11Device();
    ID3D11DeviceContext* const context = D3D11DeviceContext();
    if (!device || !context || FAILED(device->GetDeviceRemovedReason()))
    {
        ReleaseGpu(*atlas);
        return;
    }
    if (atlas->device != device)
    {
        ReleaseGpu(*atlas);
        atlas->registrationFailed = false;
    }
    if (atlas->registrationFailed)
        return;
    if (!atlas->texture)
    {
        D3D11_TEXTURE2D_DESC description{};
        description.Width = atlas->width;
        description.Height = atlas->height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{};
        initial.pSysMem = atlas->pixels.data();
        initial.SysMemPitch = atlas->width * 4;
        if (FAILED(device->CreateTexture2D(&description, &initial, &atlas->texture)) ||
            FAILED(device->CreateShaderResourceView(atlas->texture.Get(), nullptr, &atlas->view)))
        {
            ReleaseGpu(*atlas);
            return;
        }
        TextureAsset_s& asset = atlas->asset;
        asset.assetGuid = Pak_StringToGuid(atlas->name.c_str());
        asset.debugName = atlas->name.c_str();
        asset.width = atlas->width;
        asset.height = atlas->height;
        asset.imageFormat = 32;
        asset.dataSize = static_cast<uint32_t>(atlas->pixels.size());
        asset.arraySize = 1;
        asset.layerCount = 1;
        asset.permanentMipLevels = 1;
        asset.texelCount = static_cast<uint64_t>(atlas->width) * atlas->height;
        asset.streamedTextureIndex = -1;
        asset.unknownByte128 = 1;
        asset.transform[0] = asset.transform[5] = asset.transform[10] = asset.transform[15] = 1.0f;
        asset.d3d11Resource = atlas->texture.Get();
        asset.shaderResourceView = atlas->view.Get();
        const RuiImageAtlas native{
            .inverseWidth = 1.0f / atlas->width,
            .inverseHeight = 1.0f / atlas->height,
            .width = static_cast<uint16_t>(atlas->width),
            .height = static_cast<uint16_t>(atlas->height),
            .imageCount = static_cast<uint16_t>(atlas->images.size()),
            .nineSliceImageCount = 0,
            .images = atlas->entries.data(),
            .imageDimensions = atlas->dimensions.data(),
            .nineSliceData = nullptr,
            .imageNameRecords = atlas->names.data(),
            .imageNames = atlas->strings.data(),
            .texture = &asset,
            .gpuRecordBuffer = 0,
            .reserved44 = 0,
        };
        atlas->handle = RuiRegisterImageAtlas(native, atlas->records);
        if (atlas->handle == RUI_INVALID_IMAGE_ATLAS)
        {
            ReleaseGpu(*atlas);
            atlas->device = device;
            atlas->registrationFailed = true;
            spdlog::error("Script atlas '{}' could not register its image names or GPU records", atlas->name);
            return;
        }
        atlas->device = device;
        for (Image& image : atlas->images)
            image.dirty = false;
        return;
    }
    for (size_t index = 0; index < atlas->images.size(); ++index)
    {
        Image& image = atlas->images[index];
        if (!image.dirty)
            continue;
        const ScriptAtlasImage& rectangle = atlas->layout[index];
        const D3D11_BOX destination{
            .left = rectangle.x - rectangle.gutter,
            .top = rectangle.y - rectangle.gutter,
            .front = 0,
            .right = rectangle.x + rectangle.width + rectangle.gutter,
            .bottom = rectangle.y + rectangle.height + rectangle.gutter,
            .back = 1,
        };
        const size_t offset = (static_cast<size_t>(destination.top) * atlas->width + destination.left) * 4;
        context->UpdateSubresource(atlas->texture.Get(), 0, &destination, atlas->pixels.data() + offset, atlas->width * 4, 0);
        image.dirty = false;
    }
}

struct ScriptAtlasDownload
{
    std::vector<uint8_t>* bytes;
    const std::atomic_bool* cancelled;
    const std::atomic_bool* stopped;
    std::exception_ptr exception;

    static size_t Write(char* data, size_t size, size_t count, void* context) noexcept
    {
        auto& request = *static_cast<ScriptAtlasDownload*>(context);
        if (request.cancelled->load() || request.stopped->load() || size && count > SCRIPT_ATLAS_MAX_ENCODED / size)
            return 0;
        const size_t length = size * count;
        if (length > SCRIPT_ATLAS_MAX_ENCODED - request.bytes->size())
            return 0;
        try
        {
            request.bytes->insert(request.bytes->end(), reinterpret_cast<uint8_t*>(data), reinterpret_cast<uint8_t*>(data) + length);
            return length;
        }
        catch (...)
        {
            request.exception = std::current_exception();
            return 0;
        }
    }
    static int Progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept
    {
        const auto& request = *static_cast<ScriptAtlasDownload*>(context);
        return request.cancelled->load() || request.stopped->load();
    }
};

bool CScriptAtlasManager::Impl::Download(const Job& job, std::vector<uint8_t>& bytes)
{
    if (IsHttpDisabled())
        return false;
    std::string url = job.source;
    for (size_t redirect = 0; redirect <= 3 && !Cancelled(job); ++redirect)
    {
        using UrlPtr = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;
        using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
        using ListPtr = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
        UrlPtr parsed(curl_url(), curl_url_cleanup);
        if (!parsed || curl_url_set(parsed.get(), CURLUPART_URL, url.c_str(), CURLU_DISALLOW_USER) != CURLUE_OK)
            return false;
        char* scheme = nullptr;
        if (curl_url_get(parsed.get(), CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK)
            return false;
        const bool https = _stricmp(scheme, "https") == 0;
        curl_free(scheme);
        if (!https)
            return false;
        std::string hostname, address, port;
        ListPtr resolved(nullptr, curl_slist_free_all);
        if (!IsLocalHttpAllowed())
        {
            if (!IsHttpDestinationHostAllowed(url, hostname, address, port))
                return false;
            // Also exclude reserved ranges not comprehensively covered by the legacy helper.
            IN_ADDR ipv4{};
            if (InetPtonA(AF_INET, address.c_str(), &ipv4) != 1)
                return false;
            const auto octets = ipv4.S_un.S_un_b;
            if (octets.s_b1 >= 224 || (octets.s_b1 == 198 && (octets.s_b2 == 18 || octets.s_b2 == 19)) ||
                (octets.s_b1 == 198 && octets.s_b2 == 51 && octets.s_b3 == 100))
                return false;
            resolved.reset(curl_slist_append(nullptr, std::format("{}:{}:{}", hostname, port, address).c_str()));
            if (!resolved)
                return false;
        }
        CurlPtr curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl)
            return false;
        bytes.clear();
        ScriptAtlasDownload request{&bytes, job.cancelled.get(), &stopped, {}};
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
        curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_PROXY, "");
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 45L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(SCRIPT_ATLAS_MAX_ENCODED));
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, DisableHttpSsl() ? 0L : 1L);
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, DisableHttpSsl() ? 0L : 2L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, &ScriptAtlasDownload::Write);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &request);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, &ScriptAtlasDownload::Progress);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &request);
        if (resolved)
        {
            curl_easy_setopt(curl.get(), CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
            curl_easy_setopt(curl.get(), CURLOPT_RESOLVE, resolved.get());
        }
        const CURLcode result = curl_easy_perform(curl.get());
        if (request.exception)
            std::rethrow_exception(request.exception);
        if (result != CURLE_OK || Cancelled(job))
            return false;
        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status >= 200 && status < 300)
            return !bytes.empty();
        if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308)
            return false;
        char* destination = nullptr;
        curl_easy_getinfo(curl.get(), CURLINFO_REDIRECT_URL, &destination);
        if (!destination || std::strlen(destination) > 8192)
            return false;
        url = destination;
    }
    return false;
}

static uint64_t ScriptAtlasHash(std::string_view text, uint64_t hash)
{
    for (unsigned char value : text)
        hash = (hash ^ value) * 1099511628211ull;
    return hash;
}

void CScriptAtlasManager::Impl::Prune(const fs::path& directory)
{
    std::scoped_lock lock(cacheMutex);
    const auto now = std::chrono::steady_clock::now();
    if (lastPrune.time_since_epoch().count() && now - lastPrune < std::chrono::minutes(10))
        return;
    lastPrune = now;
    struct CachedFile
    {
        fs::path path;
        uint64_t size;
        fs::file_time_type modified;
    };
    std::vector<CachedFile> files;
    uint64_t total = 0;
    std::error_code error;
    fs::directory_iterator iterator(directory, error);
    const fs::directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error))
    {
        const auto& entry = *iterator;
        if (entry.path().extension() != ".image" || !entry.is_regular_file(error) || error)
            continue;
        const uint64_t size = entry.file_size(error);
        if (error)
            break;
        const auto modified = entry.last_write_time(error);
        if (error)
            break;
        files.push_back({entry.path(), size, modified});
        total += size;
    }
    if (error || total <= 256ull * 1024 * 1024)
        return;
    std::ranges::sort(files, {}, &CachedFile::modified);
    for (const CachedFile& file : files)
    {
        if (fs::remove(file.path, error))
            total -= file.size;
        error.clear();
        if (total <= 192ull * 1024 * 1024)
            break;
    }
}

bool CScriptAtlasManager::Impl::Encoded(const Job& job, std::vector<uint8_t>& bytes, fs::path& cachePath)
{
    if (!job.path.empty())
        return ScriptAtlasReadFile(job.path, job.root, bytes);
    if (IsHttpDisabled())
        return false;
    std::error_code error;
    fs::path directory = fs::path(GetNorthstarPrefix()) / "cache" / "scriptatlas" / "images";
    fs::create_directories(directory, error);
    if (error)
        return Download(job, bytes);
    directory = fs::canonical(directory, error);
    if (error)
        return Download(job, bytes);
    // Length-prefix the URL so arbitrary caller versions cannot alias another source.
    const std::string key = std::format("{}:{}{}", job.source.size(), job.source, job.version);
    cachePath =
        directory / std::format("{:016x}-{:016x}.image", ScriptAtlasHash(key, 1469598103934665603ull), ScriptAtlasHash(key, 1099511628211ull));
    if (ScriptAtlasReadFile(cachePath, directory, bytes))
    {
        fs::last_write_time(cachePath, fs::file_time_type::clock::now(), error);
        return true;
    }
    if (!Download(job, bytes))
        return false;
    fs::path temporary = cachePath;
    temporary += std::format(L".part.{}.{}", GetCurrentProcessId(), temporarySequence.fetch_add(1));
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output || Cancelled(job) || !MoveFileExW(temporary.c_str(), cachePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            fs::remove(temporary, error);
    }
    Prune(directory);
    return !Cancelled(job);
}

void CScriptAtlasManager::Impl::Worker()
{
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (;;)
    {
        Job job;
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [this] { return stopped.load() || !jobs.empty(); });
            if (stopped.load())
                break;
            job = std::move(jobs.front());
            jobs.pop_front();
        }
        if (Cancelled(job))
            continue;
        try
        {
            std::vector<uint8_t> encoded, pixels;
            fs::path cachePath;
            const bool success = (SUCCEEDED(apartment) || apartment == RPC_E_CHANGED_MODE) && Encoded(job, encoded, cachePath) && !Cancelled(job) &&
                                 ScriptAtlasDecode(encoded, job.atlas->layout[job.image], job.fit, pixels);
            if (!success && !Cancelled(job) && !cachePath.empty())
            {
                std::error_code ignored;
                fs::remove(cachePath, ignored);
            }
            std::scoped_lock lock(mutex);
            if (Cancelled(job) || !job.atlas->alive)
                continue;
            Atlas& atlas = *job.atlas;
            Image& image = atlas.images[job.image];
            image.state = success ? ScriptAtlasImageState::Ready : ScriptAtlasImageState::Failed;
            if (success)
            {
                const ScriptAtlasImage& rectangle = atlas.layout[job.image];
                const size_t pitch = static_cast<size_t>(rectangle.width + rectangle.gutter * 2) * 4;
                for (uint32_t row = 0; row < rectangle.height + rectangle.gutter * 2; ++row)
                {
                    const size_t destination =
                        (static_cast<size_t>(rectangle.y - rectangle.gutter + row) * atlas.width + rectangle.x - rectangle.gutter) * 4;
                    std::memcpy(atlas.pixels.data() + destination, pixels.data() + static_cast<size_t>(row) * pitch, pitch);
                }
                image.dirty = true;
                QueueRender(job.atlas);
            }
        }
        catch (const std::bad_alloc&)
        {
            // Preserve fatal allocation handling on the producer thread rather than escaping the worker entry point.
            const std::exception_ptr exception = std::current_exception();
            g_TaskQueue.Dispatch([exception] { std::rethrow_exception(exception); });
        }
        catch (...)
        {
            std::scoped_lock lock(mutex);
            if (!Cancelled(job) && job.atlas->alive)
                job.atlas->images[job.image].state = ScriptAtlasImageState::Failed;
        }
    }
    if (SUCCEEDED(apartment))
        CoUninitialize();
}

CScriptAtlasManager::CScriptAtlasManager() : m_Impl(new Impl)
{
}

CScriptAtlasManager& CScriptAtlasManager::Get()
{
    // Engine callbacks may outlive static destruction. Shutdown retires resources explicitly.
    static CScriptAtlasManager* manager = new CScriptAtlasManager;
    return *manager;
}

static bool ScriptAtlasValidName(const std::string& name)
{
    if (name.empty() || name.size() > 255)
        return false;
    for (unsigned char character : name)
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
              character == '_' || character == '-' || character == '/' || character == '.'))
            return false;
    return true;
}

int CScriptAtlasManager::Create(uintptr_t owner, std::string name, uint32_t width, uint32_t height, std::vector<ScriptAtlasImage> images,
                                std::string& error)
{
    error.clear();
    const auto fail = [&error](const char* message)
    {
        error = message;
        return 0;
    };
    if (!owner || IsDedicatedServer() || m_Impl->stopped.load())
        return fail("Script atlases are unavailable in this context");
    if (!ScriptAtlasValidName(name))
        return fail("Atlas name must contain 1-255 asset-path characters");
    if (!ScriptAtlasValidDimensions(width, height))
        return fail("Atlas dimensions must be 1-8192 with at most 64M pixels");
    if (images.empty() || images.size() > SCRIPT_ATLAS_MAX_IMAGES)
        return fail("An atlas must contain between 1 and 2048 images");
    size_t stringBytes = 0;
    std::unordered_set<uint32_t> hashes;
    for (size_t index = 0; index < images.size(); ++index)
    {
        const ScriptAtlasImage& image = images[index];
        if (!ScriptAtlasValidName(image.name))
            return fail("Image names must contain 1-255 asset-path characters");
        stringBytes += image.name.size() + 1;
        if (stringBytes > 65536)
            return fail("Atlas image name table exceeds the native 64KiB limit");
        const uint64_t guid = Pak_StringToGuid(image.name.c_str());
        if (!hashes.insert(static_cast<uint32_t>(guid) ^ static_cast<uint32_t>(guid >> 32)).second)
            return fail("Atlas image names or their native hashes collide");
        if (!image.width || !image.height || image.x < image.gutter || image.y < image.gutter ||
            static_cast<uint64_t>(image.x) + image.width + image.gutter > width ||
            static_cast<uint64_t>(image.y) + image.height + image.gutter > height)
            return fail("Every image and its gutter must fit completely inside the atlas");
        for (size_t previous = 0; previous < index; ++previous)
        {
            const ScriptAtlasImage& other = images[previous];
            if (image.x - image.gutter < other.x + other.width + other.gutter && other.x - other.gutter < image.x + image.width + image.gutter &&
                image.y - image.gutter < other.y + other.height + other.gutter && other.y - other.gutter < image.y + image.height + image.gutter)
                return fail("Atlas image rectangles, including gutters, must not overlap");
        }
    }
    std::scoped_lock lock(m_Impl->mutex);
    if (m_Impl->stopped.load())
        return fail("Script atlas manager has shut down");
    if (m_Impl->nextId > static_cast<uint64_t>(std::numeric_limits<int>::max()))
        return fail("Script atlas handle space is exhausted");
    const size_t bytes = static_cast<size_t>(width) * height * 4;
    if (m_Impl->atlases.size() >= SCRIPT_ATLAS_MAX_ATLASES || bytes > SCRIPT_ATLAS_MEMORY_BUDGET - m_Impl->allocated)
        return fail("Script atlas count or 256MiB pixel budget is exhausted");
    const uint64_t textureGuid = Pak_StringToGuid(name.c_str());
    size_t imageCount = images.size();
    size_t pendingNames = images.size();
    std::unordered_set<uint32_t> retiringNames;
    for (const auto& [id, existing] : m_Impl->atlases)
    {
        if (!existing->alive)
        {
            for (const auto& record : existing->names)
                retiringNames.insert(record.nameHash);
            continue;
        }
        imageCount += existing->images.size();
        if (Pak_StringToGuid(existing->name.c_str()) == textureGuid)
            return fail("Atlas texture name is already owned by another live atlas");
        for (const auto& record : existing->names)
            if (hashes.contains(record.nameHash))
                return fail("Image name is already owned by another live atlas");
        if (existing->handle == RUI_INVALID_IMAGE_ATLAS)
            pendingNames += existing->images.size();
    }
    if (imageCount > SCRIPT_ATLAS_MAX_IMAGES)
        return fail("The total script atlas image limit is 2048");
    if (!g_RuiImageDescriptorMap || !RuiImageAtlas_CreateGpuBuffer || !RuiImageAtlas_DestroyGpuBuffer)
        return fail("The native RUI atlas registry is not ready");
    AcquireSRWLockShared(&g_RuiImageDescriptorMap->lock);
    bool collision = false;
    for (uint32_t hash : hashes)
        if (g_RuiImageDescriptorMap->Find(hash) && !retiringNames.contains(hash))
            collision = true;
    const size_t availableNames = g_RuiImageDescriptorMap->entryCapacity - g_RuiImageDescriptorMap->liveEntryCount;
    ReleaseSRWLockShared(&g_RuiImageDescriptorMap->lock);
    if (collision)
        return fail("Image name collides with an existing native RUI asset");
    if (pendingNames > availableNames)
        return fail("Native RUI image descriptor capacity is exhausted");

    auto atlas = std::make_shared<Impl::Atlas>();
    atlas->id = static_cast<int>(m_Impl->nextId++);
    atlas->owner = owner;
    atlas->name = std::move(name);
    atlas->width = width;
    atlas->height = height;
    atlas->layout = std::move(images);
    atlas->pixels.resize(bytes);
    atlas->images.resize(atlas->layout.size());
    atlas->entries.reserve(atlas->layout.size());
    atlas->dimensions.reserve(atlas->layout.size());
    atlas->names.reserve(atlas->layout.size());
    atlas->strings.reserve(stringBytes);
    atlas->records.reserve(atlas->layout.size());
    for (const ScriptAtlasImage& image : atlas->layout)
    {
        atlas->entries.push_back({{-0.0f, -0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}});
        atlas->dimensions.push_back({static_cast<uint16_t>(image.width), static_cast<uint16_t>(image.height)});
        const uint64_t guid = Pak_StringToGuid(image.name.c_str());
        atlas->names.push_back({static_cast<uint32_t>(guid) ^ static_cast<uint32_t>(guid >> 32), 0, static_cast<uint16_t>(atlas->strings.size())});
        atlas->strings.insert(atlas->strings.end(), image.name.begin(), image.name.end());
        atlas->strings.push_back('\0');
        atlas->records.push_back({{static_cast<float>(image.x) / width, static_cast<float>(image.y) / height},
                                  {static_cast<float>(image.width) / width, static_cast<float>(image.height) / height}});
    }
    m_Impl->atlases.emplace(atlas->id, atlas);
    m_Impl->allocated += bytes;
    m_Impl->QueueRender(atlas);
    m_Impl->QueueMaintenance();
    return atlas->id;
}

bool CScriptAtlasManager::Destroy(uintptr_t owner, int id)
{
    std::scoped_lock lock(m_Impl->mutex);
    const auto atlas = m_Impl->Find(owner, id);
    if (!atlas)
        return false;
    m_Impl->Retire(atlas);
    return true;
}

void CScriptAtlasManager::ReleaseOwner(uintptr_t owner)
{
    std::scoped_lock lock(m_Impl->mutex);
    for (const auto& [id, atlas] : m_Impl->atlases)
        if (atlas->alive && atlas->owner == owner)
            m_Impl->Retire(atlas);
}

std::string CScriptAtlasManager::ImageName(uintptr_t owner, int id, size_t image) const
{
    std::scoped_lock lock(m_Impl->mutex);
    const auto atlas = m_Impl->Find(owner, id);
    return atlas && image < atlas->layout.size() ? atlas->layout[image].name : std::string();
}

bool CScriptAtlasManager::LoadImage(uintptr_t owner, int id, size_t image, std::string source, std::string version, ScriptAtlasFit fit)
{
    if (source.empty() || source.size() > 8192 || source.find('\0') != std::string::npos || version.size() > 4096 ||
        (fit != ScriptAtlasFit::Cover && fit != ScriptAtlasFit::Contain && fit != ScriptAtlasFit::Stretch))
        return false;
    Impl::Job job{};
    job.image = image;
    job.source = std::move(source);
    job.version = std::move(version);
    job.fit = fit;
    if (_strnicmp(job.source.c_str(), "https://", 8) != 0)
    {
        try
        {
            if (!ScriptAtlasResolveLocal(job.source, job.path, job.root))
                return false;
        }
        catch (const std::bad_alloc&)
        {
            throw;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
    else if (IsHttpDisabled())
        return false;
    std::scoped_lock lock(m_Impl->mutex);
    const auto atlas = m_Impl->Find(owner, id);
    if (!atlas || image >= atlas->images.size() || m_Impl->stopped.load())
        return false;
    job.atlas = atlas;
    job.cancelled = std::make_shared<std::atomic_bool>(false);
    if (m_Impl->workers.empty())
    {
        m_Impl->workers.reserve(2);
        try
        {
            for (size_t index = 0; index < 2; ++index)
                m_Impl->workers.emplace_back([impl = m_Impl]
                {
                    try
                    {
                        impl->Worker();
                    }
                    catch (const std::bad_alloc&)
                    {
                        const std::exception_ptr exception = std::current_exception();
                        try
                        {
                            g_TaskQueue.Dispatch([exception] { std::rethrow_exception(exception); });
                        }
                        catch (...)
                        {
                            RaiseFailFastException(nullptr, nullptr, 0);
                        }
                    }
                    catch (...)
                    {
                        // Synchronization failures are not image errors; invalidate pending work without an uncaught thread exception.
                        std::scoped_lock lock(impl->mutex);
                        for (const auto& [id, pending] : impl->atlases)
                            for (Impl::Image& state : pending->images)
                                if (state.state == ScriptAtlasImageState::Loading)
                                    state.state = ScriptAtlasImageState::Failed;
                    }
                });
        }
        catch (const std::bad_alloc&)
        {
            throw;
        }
        catch (const std::exception& exception)
        {
            spdlog::error("Could not start a script atlas worker: {}", exception.what());
            if (m_Impl->workers.empty())
                return false;
        }
    }
    Impl::Image& state = atlas->images[image];
    if (state.cancelled)
        state.cancelled->store(true);
    state.cancelled = job.cancelled;
    state.state = ScriptAtlasImageState::Loading;
    std::erase_if(m_Impl->jobs, [&](const Impl::Job& pending) { return pending.atlas == atlas && pending.image == image; });
    m_Impl->jobs.push_back(std::move(job));
    m_Impl->changed.notify_one();
    return true;
}

bool CScriptAtlasManager::ClearImage(uintptr_t owner, int id, size_t image, uint32_t rgba)
{
    std::scoped_lock lock(m_Impl->mutex);
    const auto atlas = m_Impl->Find(owner, id);
    if (!atlas || image >= atlas->images.size())
        return false;
    Impl::Image& state = atlas->images[image];
    if (state.cancelled)
        state.cancelled->store(true);
    state.cancelled.reset();
    state.state = ScriptAtlasImageState::Empty;
    state.dirty = true;
    std::erase_if(m_Impl->jobs, [&](const Impl::Job& pending) { return pending.atlas == atlas && pending.image == image; });
    const ScriptAtlasImage& rectangle = atlas->layout[image];
    for (uint32_t y = rectangle.y - rectangle.gutter; y < rectangle.y + rectangle.height + rectangle.gutter; ++y)
    {
        uint8_t* row = atlas->pixels.data() + (static_cast<size_t>(y) * atlas->width + rectangle.x - rectangle.gutter) * 4;
        for (uint32_t x = 0; x < rectangle.width + rectangle.gutter * 2; ++x)
        {
            row[x * 4] = static_cast<uint8_t>(rgba);
            row[x * 4 + 1] = static_cast<uint8_t>(rgba >> 8);
            row[x * 4 + 2] = static_cast<uint8_t>(rgba >> 16);
            row[x * 4 + 3] = static_cast<uint8_t>(rgba >> 24);
        }
    }
    m_Impl->QueueRender(atlas);
    return true;
}

ScriptAtlasImageState CScriptAtlasManager::ImageState(uintptr_t owner, int id, size_t image) const
{
    std::scoped_lock lock(m_Impl->mutex);
    const auto atlas = m_Impl->Find(owner, id);
    if (!atlas || image >= atlas->images.size())
        return ScriptAtlasImageState::Failed;
    ID3D11Device* const device = D3D11Device();
    if (atlas->registrationFailed && atlas->device == device && device && SUCCEEDED(device->GetDeviceRemovedReason()))
        return ScriptAtlasImageState::Failed;
    const bool ready = atlas->handle != RUI_INVALID_IMAGE_ATLAS && atlas->device == device && D3D11DeviceContext() && device &&
                       SUCCEEDED(device->GetDeviceRemovedReason());
    if (!ready)
        m_Impl->QueueRender(atlas);
    const Impl::Image& state = atlas->images[image];
    if (state.state == ScriptAtlasImageState::Ready && (!ready || state.dirty))
        return ScriptAtlasImageState::Loading;
    return state.state;
}

void CScriptAtlasManager::Shutdown()
{
    std::vector<std::thread> workers;
    {
        std::scoped_lock lock(m_Impl->mutex);
        if (m_Impl->stopped.exchange(true))
            return;
        for (const auto& [id, atlas] : m_Impl->atlases)
            if (atlas->alive)
                m_Impl->Retire(atlas);
        m_Impl->jobs.clear();
        workers.swap(m_Impl->workers);
    }
    m_Impl->changed.notify_all();
    for (std::thread& worker : workers)
        if (worker.joinable())
            worker.join();
}
