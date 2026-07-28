#pragma once

#include "materialsystem/cmaterialglue.h"
#include "rtech/rui/imageatlas.h"

#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

class CWorkshopThumbnailAtlas final
{
public:
	static constexpr size_t SLOT_COUNT = 24;
	static constexpr uint32_t ATLAS_COLUMNS = 4;
	static constexpr uint32_t GUTTER = 4;
	static constexpr uint32_t CELL_WIDTH = 512;
	static constexpr uint32_t IMAGE_WIDTH = CELL_WIDTH - GUTTER * 2;
	static constexpr uint32_t IMAGE_HEIGHT = IMAGE_WIDTH / 2;
	static constexpr uint32_t CELL_HEIGHT = IMAGE_HEIGHT + GUTTER * 2;
	static constexpr uint32_t ATLAS_ROWS = (static_cast<uint32_t>(SLOT_COUNT) + ATLAS_COLUMNS - 1) / ATLAS_COLUMNS;
	static constexpr uint32_t ATLAS_WIDTH = ATLAS_COLUMNS * CELL_WIDTH;
	static constexpr uint32_t ATLAS_HEIGHT = ATLAS_ROWS * CELL_HEIGHT;

	static CWorkshopThumbnailAtlas& Get()
	{
		static CWorkshopThumbnailAtlas* s_pInstance = new CWorkshopThumbnailAtlas;
		return *s_pInstance;
	}

	bool Initialize();
	bool IsReady() const;
	bool FillChecker(size_t slot);
	bool FillPlaceholder(size_t slot, bool failed = false);
	bool UpdateSlotRgba(size_t slot, std::span<const uint8_t> rgba, uint32_t rowPitch = CELL_WIDTH * 4);
	const char* GetAssetPath(size_t slot) const noexcept
	{
		return slot < ASSET_PATHS.size() ? ASSET_PATHS[slot].data() : "";
	}
	void Shutdown();

	CWorkshopThumbnailAtlas(const CWorkshopThumbnailAtlas&) = delete;
	CWorkshopThumbnailAtlas& operator=(const CWorkshopThumbnailAtlas&) = delete;

private:
	CWorkshopThumbnailAtlas();
	~CWorkshopThumbnailAtlas() = delete;

	using AssetPath = std::array<char, 48>;
	static const std::array<AssetPath, SLOT_COUNT> ASSET_PATHS;
	static void SetPixel(uint8_t* pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);

	static const std::array<const char*, 4> TEXTURE_TEMPLATE_IMAGES;
	static const char TEXTURE_NAME[];

	bool InitializeLocked();
	bool UploadSlotLocked(size_t slot, const uint8_t* rgba, uint32_t rowPitch);
	void ReleaseLocked();

	mutable std::mutex m_Mutex;
	CImageAtlas m_Atlas;
	RpakTextureHeader m_TextureHeader{};
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_Texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ShaderResourceView;
	ID3D11Device* m_Device = nullptr;
	std::vector<uint8_t> m_CellScratch;
	char m_TextureName[64]{};
};
