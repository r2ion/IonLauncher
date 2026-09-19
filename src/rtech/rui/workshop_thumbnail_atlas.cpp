#include "rtech/rui/workshop_thumbnail_atlas.h"

#include "materialsystem/cmatqueuedrendercontext.h"
#include "rtech/paktools.h"
#include "windows/id3dx.h"

#include <cstdio>

static constexpr char g_WorkshopTextureAsset[] = "texture/ns/modworkshop_thumbnail_atlas";

static void SetPixel(uint8_t* pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
{
    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = alpha;
}

CWorkshopThumbnailAtlas::CWorkshopThumbnailAtlas() : m_CellScratch(static_cast<size_t>(CELL_WIDTH) * CELL_HEIGHT * 4)
{
}

bool CWorkshopThumbnailAtlas::Initialize()
{
    if (!ThreadInRenderThread())
    {
        spdlog::error("ModWorkshop thumbnail atlas must be initialized on the material render thread");
        return false;
    }
    std::scoped_lock lock(m_TextureMutex);
    return InitializeLocked();
}

bool CWorkshopThumbnailAtlas::IsReady() const
{
    std::scoped_lock lock(m_TextureMutex);
    return m_AtlasHandle != RUI_INVALID_IMAGE_ATLAS && m_Texture && m_ShaderResourceView && m_Device;
}

bool CWorkshopThumbnailAtlas::InitializeLocked()
{
    ID3D11Device* const device = D3D11Device();
    if (!device || !D3D11DeviceContext())
    {
        ReleaseLocked();
        return false;
    }

    if (m_Device == device && m_Texture && m_ShaderResourceView && m_AtlasHandle != RUI_INVALID_IMAGE_ATLAS)
        return true;

    ReleaseLocked();

    const std::vector<uint32_t> initialPixels(static_cast<size_t>(ATLAS_WIDTH) * ATLAS_HEIGHT, 0xFF1F1B18u);
    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = initialPixels.data();
    initialData.SysMemPitch = ATLAS_WIDTH * 4;

    D3D11_TEXTURE2D_DESC description{};
    description.Width = ATLAS_WIDTH;
    description.Height = ATLAS_HEIGHT;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    HRESULT result = device->CreateTexture2D(&description, &initialData, m_Texture.GetAddressOf());
    if (FAILED(result))
    {
        spdlog::error("ModWorkshop thumbnail atlas texture creation failed: 0x{:08X}", static_cast<uint32_t>(result));
        ReleaseLocked();
        return false;
    }

    result = device->CreateShaderResourceView(m_Texture.Get(), nullptr, m_ShaderResourceView.GetAddressOf());
    if (FAILED(result))
    {
        spdlog::error("ModWorkshop thumbnail atlas SRV creation failed: 0x{:08X}", static_cast<uint32_t>(result));
        ReleaseLocked();
        return false;
    }

    m_TextureAsset.assetGuid = Pak_StringToGuid(g_WorkshopTextureAsset);
    m_TextureAsset.debugName = g_WorkshopTextureAsset;
    m_TextureAsset.width = ATLAS_WIDTH;
    m_TextureAsset.height = ATLAS_HEIGHT;
    // Native TextureAsset format table (materialsystem + 0x1C3280), not DXGI's enum.
    m_TextureAsset.imageFormat = 32; // RGBA8 sRGB; depth remains zero for a 2D texture.
    m_TextureAsset.dataSize = ATLAS_WIDTH * ATLAS_HEIGHT * 4;
    m_TextureAsset.arraySize = 1;
    m_TextureAsset.layerCount = 1;
    m_TextureAsset.permanentMipLevels = 1;
    m_TextureAsset.texelCount = static_cast<uint64_t>(ATLAS_WIDTH) * ATLAS_HEIGHT;
    m_TextureAsset.streamedTextureIndex = -1;
    m_TextureAsset.unknownByte128 = 1; // Resident mip count set by the native 2D loader.
    m_TextureAsset.transform[0] = 1.0f;
    m_TextureAsset.transform[5] = 1.0f;
    m_TextureAsset.transform[10] = 1.0f;
    m_TextureAsset.transform[15] = 1.0f;
    m_TextureAsset.d3d11Resource = m_Texture.Get();
    m_TextureAsset.shaderResourceView = m_ShaderResourceView.Get();

    constexpr float inverseWidth = 1.0f / ATLAS_WIDTH;
    constexpr float inverseHeight = 1.0f / ATLAS_HEIGHT;
    for (size_t slot = 0; slot < SLOT_COUNT; ++slot)
    {
        const float u = (static_cast<uint32_t>(slot % ATLAS_COLUMNS) * CELL_WIDTH + GUTTER) * inverseWidth;
        const float v = (static_cast<uint32_t>(slot / ATLAS_COLUMNS) * CELL_HEIGHT + GUTTER) * inverseHeight;
        // CPU clipping stays in image-local UV space; the GPU record maps it into the texture.
        m_Images[slot] = {{-0.0f, -0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}};
        m_ImageDimensions[slot] = {IMAGE_WIDTH, IMAGE_HEIGHT};
        m_GpuRecords[slot] = {{u, v}, {IMAGE_WIDTH * inverseWidth, IMAGE_HEIGHT * inverseHeight}};
        std::snprintf(m_ImageNames[slot].data(), m_ImageNames[slot].size(), "rui/ns/modworkshop/card_%zu", slot);
        const uint64_t guid = Pak_StringToGuid(m_ImageNames[slot].data());
        m_ImageNameRecords[slot] = {static_cast<uint32_t>(guid) ^ static_cast<uint32_t>(guid >> 32), 0,
                                    static_cast<uint16_t>(slot * m_ImageNames[slot].size())};
    }

    const RuiImageAtlas atlas{
        .inverseWidth = inverseWidth,
        .inverseHeight = inverseHeight,
        .width = ATLAS_WIDTH,
        .height = ATLAS_HEIGHT,
        .imageCount = SLOT_COUNT,
        .nineSliceImageCount = 0,
        .images = m_Images.data(),
        .imageDimensions = m_ImageDimensions.data(),
        .nineSliceData = nullptr,
        .imageNameRecords = m_ImageNameRecords.data(),
        .imageNames = m_ImageNames[0].data(),
        .texture = &m_TextureAsset,
        .gpuRecordBuffer = 0,
        .reserved44 = 0,
    };
    m_AtlasHandle = RuiRegisterImageAtlas(atlas, m_GpuRecords);
    if (m_AtlasHandle == RUI_INVALID_IMAGE_ATLAS)
    {
        spdlog::error("ModWorkshop thumbnail atlas registration failed");
        ReleaseLocked();
        return false;
    }

    m_Device = device;
    return true;
}

bool CWorkshopThumbnailAtlas::UploadSlotLocked(size_t slot, const uint8_t* rgba, uint32_t rowPitch)
{
    if (slot >= SLOT_COUNT || !rgba || !m_Texture)
        return false;
    ID3D11DeviceContext* const context = D3D11DeviceContext();
    if (!context || D3D11Device() != m_Device)
        return false;

    const uint32_t cellX = static_cast<uint32_t>(slot % ATLAS_COLUMNS) * CELL_WIDTH;
    const uint32_t cellY = static_cast<uint32_t>(slot / ATLAS_COLUMNS) * CELL_HEIGHT;
    const D3D11_BOX destination{
        .left = cellX,
        .top = cellY,
        .front = 0,
        .right = cellX + CELL_WIDTH,
        .bottom = cellY + CELL_HEIGHT,
        .back = 1,
    };
    context->UpdateSubresource(m_Texture.Get(), 0, &destination, rgba, rowPitch, 0);
    return true;
}

bool CWorkshopThumbnailAtlas::UpdateSlotRgba(size_t slot, std::span<const uint8_t> rgba, uint32_t rowPitch)
{
    if (!ThreadInRenderThread() || slot >= SLOT_COUNT || rowPitch < CELL_WIDTH * 4)
        return false;
    const size_t requiredBytes = static_cast<size_t>(rowPitch) * (CELL_HEIGHT - 1) + static_cast<size_t>(CELL_WIDTH) * 4;
    if (rgba.size() < requiredBytes)
        return false;

    std::scoped_lock lock(m_TextureMutex);
    if (!InitializeLocked())
        return false;
    return UploadSlotLocked(slot, rgba.data(), rowPitch);
}

bool CWorkshopThumbnailAtlas::FillPlaceholder(size_t slot, bool failed)
{
    if (!ThreadInRenderThread() || slot >= SLOT_COUNT)
        return false;
    std::scoped_lock lock(m_TextureMutex);
    if (!InitializeLocked())
        return false;

    for (uint32_t y = 0; y < CELL_HEIGHT; ++y)
    {
        for (uint32_t x = 0; x < CELL_WIDTH; ++x)
        {
            const bool border = x < GUTTER || y < GUTTER || x >= CELL_WIDTH - GUTTER || y >= CELL_HEIGHT - GUTTER;
            uint8_t* pixel = m_CellScratch.data() + (static_cast<size_t>(y) * CELL_WIDTH + x) * 4;
            if (failed)
                SetPixel(pixel, border ? 77 : 45, border ? 24 : 19, border ? 27 : 23);
            else
                SetPixel(pixel, border ? 48 : 33, border ? 52 : 37, border ? 58 : 42);
        }
    }
    return UploadSlotLocked(slot, m_CellScratch.data(), CELL_WIDTH * 4);
}

void CWorkshopThumbnailAtlas::ReleaseLocked()
{
    if (m_AtlasHandle != RUI_INVALID_IMAGE_ATLAS)
    {
        RuiUnregisterImageAtlas(m_AtlasHandle);
        m_AtlasHandle = RUI_INVALID_IMAGE_ATLAS;
    }
    m_TextureAsset = {};
    m_ShaderResourceView.Reset();
    m_Texture.Reset();
    m_Device = nullptr;
}
