#include "rtech/rui/workshop_thumbnail_atlas.h"

#include "materialsystem/dx11_device.h"
#include "rtech/rui/render.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

const std::array<CWorkshopThumbnailAtlas::AssetPath, CWorkshopThumbnailAtlas::SLOT_COUNT> CWorkshopThumbnailAtlas::ASSET_PATHS = []
{
	std::array<AssetPath, CWorkshopThumbnailAtlas::SLOT_COUNT> result{};
	for (size_t slot = 0; slot < result.size(); ++slot)
		std::snprintf(result[slot].data(), result[slot].size(), "rui/ns/modworkshop/card_%zu", slot);
	return result;
}();

const std::array<const char*, 4> CWorkshopThumbnailAtlas::TEXTURE_TEMPLATE_IMAGES = {
    "rui/menu/common/dialog_gradient", "rui/menu/common/appearance_button_swatch", "rui/menu/common/merit_state_success",
    "rui/menu/common/lobby_icon_owner"};
const char CWorkshopThumbnailAtlas::TEXTURE_NAME[] = "texture/ns/modworkshop_thumbnail_atlas";

void CWorkshopThumbnailAtlas::SetPixel(uint8_t* pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
	pixel[0] = red;
	pixel[1] = green;
	pixel[2] = blue;
	pixel[3] = alpha;
}

CWorkshopThumbnailAtlas::CWorkshopThumbnailAtlas() : m_CellScratch(static_cast<size_t>(CELL_WIDTH) * CELL_HEIGHT * 4)
{
	strcpy_s(m_TextureName, TEXTURE_NAME);
}

bool CWorkshopThumbnailAtlas::Initialize()
{
	if (!CRuiRenderTaskQueue::Get().IsCurrentThread())
	{
		spdlog::error("ModWorkshop thumbnail atlas must be initialized on the material render thread");
		return false;
	}
	std::scoped_lock lock(m_Mutex);
	return InitializeLocked();
}

bool CWorkshopThumbnailAtlas::InitializeLocked()
{
	const CDx11Device::Snapshot device = CDx11Device::GetSnapshot();
	if (!device || !CImageAtlas::AreRuntimeBindingsReady())
		return false;

	if (m_Device == device.m_pDevice && m_Atlas.IsResident())
		return m_Atlas.EnsureRegistered();

	if (m_Device || m_Atlas.IsResident())
	{
		spdlog::info("Recreating ModWorkshop thumbnail atlas after D3D device change");
		ReleaseLocked();
	}

	std::vector<uint8_t> initialPixels(static_cast<size_t>(ATLAS_WIDTH) * ATLAS_HEIGHT * 4);
	for (size_t pixelIndex = 0; pixelIndex < initialPixels.size(); pixelIndex += 4)
		SetPixel(initialPixels.data() + pixelIndex, 24, 27, 31);
	for (uint32_t y = 0; y < CELL_HEIGHT; ++y)
	{
		for (uint32_t x = 0; x < CELL_WIDTH; ++x)
		{
			const bool magenta = ((x / 16) + (y / 16)) % 2 == 0;
			uint8_t* pixel = initialPixels.data() + (static_cast<size_t>(y) * ATLAS_WIDTH + x) * 4;
			SetPixel(pixel, magenta ? 255 : 0, 0, magenta ? 255 : 0);
		}
	}

	D3D11_TEXTURE2D_DESC textureDescription{};
	textureDescription.Width = ATLAS_WIDTH;
	textureDescription.Height = ATLAS_HEIGHT;
	textureDescription.MipLevels = 1;
	textureDescription.ArraySize = 1;
	textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	textureDescription.SampleDesc.Count = 1;
	textureDescription.Usage = D3D11_USAGE_DEFAULT;
	textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = initialPixels.data();
	initialData.SysMemPitch = ATLAS_WIDTH * 4;
	HRESULT result = device.m_pDevice->CreateTexture2D(&textureDescription, &initialData, m_Texture.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		spdlog::error("Failed creating ModWorkshop thumbnail texture: 0x{:08X}", static_cast<uint32_t>(result));
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
	viewDescription.Format = textureDescription.Format;
	viewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDescription.Texture2D.MostDetailedMip = 0;
	viewDescription.Texture2D.MipLevels = 1;
	result = device.m_pDevice->CreateShaderResourceView(m_Texture.Get(), &viewDescription, m_ShaderResourceView.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		spdlog::error("Failed creating ModWorkshop thumbnail SRV: 0x{:08X}", static_cast<uint32_t>(result));
		ReleaseLocked();
		return false;
	}

	const RpakTextureHeader* templateTexture = nullptr;
	for (const char* path : TEXTURE_TEMPLATE_IMAGES)
	{
		const RuiImageAssetDescriptor* descriptor = CImageAtlas::FindAssetDescriptor(CImageAtlas::HashImagePath(path));
		RuiImageAtlas* atlas = descriptor ? CImageAtlas::GetAtlas(descriptor->atlasIndex) : nullptr;
		if (!atlas || !atlas->texture)
			continue;
		templateTexture = static_cast<const RpakTextureHeader*>(atlas->texture);
		break;
	}

	m_TextureHeader = templateTexture ? *templateTexture : RpakTextureHeader{};
	if (!templateTexture)
		spdlog::warn("No loaded UI texture template was available for the ModWorkshop atlas");
	m_TextureHeader.guid = CImageAtlas::HashAssetPath(m_TextureName);
	m_TextureHeader.name = m_TextureName;
	m_TextureHeader.width = static_cast<uint16_t>(ATLAS_WIDTH);
	m_TextureHeader.height = static_cast<uint16_t>(ATLAS_HEIGHT);
	m_TextureHeader.depth = 0;
	m_TextureHeader.dxgiFormat = static_cast<uint16_t>(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	m_TextureHeader.dataSize = ATLAS_WIDTH * ATLAS_HEIGHT * 4;
	m_TextureHeader.compressionType = 0;
	m_TextureHeader.optStreamedMipCount = 0;
	m_TextureHeader.arraySize = 0;
	m_TextureHeader.layerCount = 0;
	m_TextureHeader.mipFlags = 0;
	m_TextureHeader.permanentMipCount = 1;
	m_TextureHeader.streamedMipCount = 0;
	std::memset(m_TextureHeader.unk, 0, sizeof(m_TextureHeader.unk));
	m_TextureHeader.numPixels = 0;
	m_TextureHeader.d3d11Resource = m_Texture.Get();
	m_TextureHeader.shaderResourceView = m_ShaderResourceView.Get();

	std::array<CImageAtlas::Image, SLOT_COUNT> images{};
	for (size_t slot = 0; slot < images.size(); ++slot)
	{
		const uint32_t cellX = static_cast<uint32_t>(slot % ATLAS_COLUMNS) * CELL_WIDTH;
		const uint32_t cellY = static_cast<uint32_t>(slot / ATLAS_COLUMNS) * CELL_HEIGHT;
		images[slot] = {.m_NameHash = CImageAtlas::HashImagePath(ASSET_PATHS[slot].data()),
		                .m_PosX = cellX + GUTTER,
		                .m_PosY = cellY + GUTTER,
		                .m_Width = IMAGE_WIDTH,
		                .m_Height = IMAGE_HEIGHT};
	}

	if (!m_Atlas.Create(static_cast<uint16_t>(ATLAS_WIDTH), static_cast<uint16_t>(ATLAS_HEIGHT), &m_TextureHeader, images))
	{
		ReleaseLocked();
		return false;
	}

	m_Device = device.m_pDevice;
	return true;
}

bool CWorkshopThumbnailAtlas::IsReady() const
{
	std::scoped_lock lock(m_Mutex);
	return m_Device && m_Atlas.IsResident() && m_Texture && m_ShaderResourceView;
}

bool CWorkshopThumbnailAtlas::UploadSlotLocked(size_t slot, const uint8_t* rgba, uint32_t rowPitch)
{
	if (slot >= SLOT_COUNT || !rgba || !m_Texture)
		return false;
	const CDx11Device::Snapshot device = CDx11Device::GetSnapshot();
	if (!device || device.m_pDevice != m_Device)
		return false;

	const uint32_t cellX = static_cast<uint32_t>(slot % ATLAS_COLUMNS) * CELL_WIDTH;
	const uint32_t cellY = static_cast<uint32_t>(slot / ATLAS_COLUMNS) * CELL_HEIGHT;
	D3D11_BOX destination{.left = cellX, .top = cellY, .front = 0, .right = cellX + CELL_WIDTH, .bottom = cellY + CELL_HEIGHT, .back = 1};
	device.m_pContext->UpdateSubresource(m_Texture.Get(), 0, &destination, rgba, rowPitch, 0);
	return true;
}

bool CWorkshopThumbnailAtlas::UpdateSlotRgba(size_t slot, std::span<const uint8_t> rgba, uint32_t rowPitch)
{
	if (!CRuiRenderTaskQueue::Get().IsCurrentThread() || slot >= SLOT_COUNT || rowPitch < CELL_WIDTH * 4)
		return false;
	const size_t requiredBytes = static_cast<size_t>(rowPitch) * (CELL_HEIGHT - 1) + static_cast<size_t>(CELL_WIDTH) * 4;
	if (rgba.size() < requiredBytes)
		return false;

	std::scoped_lock lock(m_Mutex);
	if (!InitializeLocked())
		return false;
	return UploadSlotLocked(slot, rgba.data(), rowPitch);
}

bool CWorkshopThumbnailAtlas::FillChecker(size_t slot)
{
	if (!CRuiRenderTaskQueue::Get().IsCurrentThread() || slot >= SLOT_COUNT)
		return false;
	std::scoped_lock lock(m_Mutex);
	if (!InitializeLocked())
		return false;

	for (uint32_t y = 0; y < CELL_HEIGHT; ++y)
	{
		for (uint32_t x = 0; x < CELL_WIDTH; ++x)
		{
			const bool magenta = ((x / 16) + (y / 16)) % 2 == 0;
			uint8_t* pixel = m_CellScratch.data() + (static_cast<size_t>(y) * CELL_WIDTH + x) * 4;
			SetPixel(pixel, magenta ? 255 : 0, 0, magenta ? 255 : 0);
		}
	}
	return UploadSlotLocked(slot, m_CellScratch.data(), CELL_WIDTH * 4);
}

bool CWorkshopThumbnailAtlas::FillPlaceholder(size_t slot, bool failed)
{
	if (!CRuiRenderTaskQueue::Get().IsCurrentThread() || slot >= SLOT_COUNT)
		return false;
	std::scoped_lock lock(m_Mutex);
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
	m_Atlas.Destroy();
	m_TextureHeader = {};
	m_ShaderResourceView.Reset();
	m_Texture.Reset();
	m_Device = nullptr;
}

void CWorkshopThumbnailAtlas::Shutdown()
{
	if (!CRuiRenderTaskQueue::Get().IsCurrentThread())
		return;
	std::scoped_lock lock(m_Mutex);
	ReleaseLocked();
}
