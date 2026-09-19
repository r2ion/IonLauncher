#pragma once

#include "rendersystem/schema/texture.g.h"
#include "rtech/rui/atlas.h"

#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

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
        static CWorkshopThumbnailAtlas* pInstance = new CWorkshopThumbnailAtlas;
        return *pInstance;
    }

    bool Initialize();
    bool IsReady() const;
    bool FillPlaceholder(size_t slot, bool failed = false);
    bool UpdateSlotRgba(size_t slot, std::span<const uint8_t> rgba, uint32_t rowPitch = CELL_WIDTH * 4);

    CWorkshopThumbnailAtlas(const CWorkshopThumbnailAtlas&) = delete;
    CWorkshopThumbnailAtlas& operator=(const CWorkshopThumbnailAtlas&) = delete;

  private:
    CWorkshopThumbnailAtlas();
    ~CWorkshopThumbnailAtlas() = delete;

    bool InitializeLocked();
    bool UploadSlotLocked(size_t slot, const uint8_t* rgba, uint32_t rowPitch);
    void ReleaseLocked();

    mutable std::mutex m_TextureMutex;
    TextureAsset_s m_TextureAsset{};
    alignas(16) std::array<RuiImageAtlasEntry, SLOT_COUNT> m_Images{};
    std::array<RuiImageDimensions, SLOT_COUNT> m_ImageDimensions{};
    std::array<RuiImageAtlasNameRecord, SLOT_COUNT> m_ImageNameRecords{};
    std::array<std::array<char, 32>, SLOT_COUNT> m_ImageNames{};
    std::array<RuiImageAtlasGpuRecord, SLOT_COUNT> m_GpuRecords{};
    RuiImageAtlasHandle m_AtlasHandle = RUI_INVALID_IMAGE_ATLAS;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_Texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ShaderResourceView;
    ID3D11Device* m_Device = nullptr;
    std::vector<uint8_t> m_CellScratch;
};
