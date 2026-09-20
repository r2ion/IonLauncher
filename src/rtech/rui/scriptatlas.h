#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ScriptAtlasFit
{
    Cover = 0,
    Contain = 1,
    Stretch = 2
};
enum class ScriptAtlasImageState
{
    Empty = 0,
    Loading = 1,
    Ready = 2,
    Failed = 3
};

struct ScriptAtlasImage
{
    std::string name;
    uint32_t x, y, width, height, gutter;
};

class CScriptAtlasManager final
{
  public:
    static CScriptAtlasManager& Get();
    int Create(uintptr_t owner, std::string name, uint32_t width, uint32_t height, std::vector<ScriptAtlasImage> images, std::string& error);
    bool Destroy(uintptr_t owner, int atlas);
    void ReleaseOwner(uintptr_t owner);
    std::string ImageName(uintptr_t owner, int atlas, size_t image) const;
    bool LoadImage(uintptr_t owner, int atlas, size_t image, std::string source, std::string version, ScriptAtlasFit fit);
    bool ClearImage(uintptr_t owner, int atlas, size_t image, uint32_t rgba);
    ScriptAtlasImageState ImageState(uintptr_t owner, int atlas, size_t image) const;
    void Shutdown();

    CScriptAtlasManager(const CScriptAtlasManager&) = delete;
    CScriptAtlasManager& operator=(const CScriptAtlasManager&) = delete;

  private:
    CScriptAtlasManager();
    ~CScriptAtlasManager() = delete;
    struct Impl;
    Impl* m_Impl;
};
