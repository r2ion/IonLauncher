#pragma once

#include "appframework/IAppSystem.h"
#include "cmodel.h"
#include "materialsystem/imaterial.h"
#include "mathlib/lightdesc.h"
#include "mathlib/vector2d.h"
#include "mathlib/vector4d.h"
#include "studio.h"
#include "tier0/fasttimer.h"
#include "tier1/utlvector.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>


struct MaterialLightingState_t
{
    float m_LightingVectors[4][4];
    bool m_LocalLightActive[4];
    std::uint16_t m_LocalLightHandles[4];
    std::uint32_t m_Flags;
};

static_assert(sizeof(MaterialLightingState_t) == 0x50);
static_assert(offsetof(MaterialLightingState_t, m_LocalLightActive) == 0x40);
static_assert(offsetof(MaterialLightingState_t, m_LocalLightHandles) == 0x44);
static_assert(offsetof(MaterialLightingState_t, m_Flags) == 0x4C);

inline constexpr std::size_t MAX_DRAW_MODEL_INFO_MATERIALS = 8;

using StudioDecalHandle_t = std::uint16_t;
struct DrawModelInfo_t
{
    studiohdr_t* m_pStudioHdr;
    studiohwdata_t* m_pHardwareData;
    void* m_pClientEntity;
    StudioDecalHandle_t m_Decals;
    std::uint8_t m_Skin;
    std::uint8_t m_Lod;
    std::uint16_t m_Body;
    std::uint16_t m_HitboxSet;
};

struct DrawModelResults_t
{
    std::int32_t m_ActualTriCount;
    std::int32_t m_TextureMemoryBytes;
    std::int32_t m_NumHardwareBones;
    std::int32_t m_NumBatches;
    std::int32_t m_NumMaterials;
    std::int32_t m_nLODUsed;
    float m_flLODMetric;
    CFastTimer m_RenderTime;
    CUtlVectorFixed<IMaterial*, MAX_DRAW_MODEL_INFO_MATERIALS> m_Materials;
};

struct GetTriangles_Vertex_t
{
    Vector3 m_Position;
    Vector3 m_Normal;
    Vector4D m_TangentS;
    Vector2D m_TexCoord;
    Vector4D m_BoneWeight;
    std::int32_t m_BoneIndex[4];
    std::int32_t m_NumBones;
};

struct GetTriangles_MaterialBatch_t
{
    IMaterial* m_pMaterial;
    CUtlVector<GetTriangles_Vertex_t> m_Verts;
    CUtlVector<std::int32_t> m_TriListIndices;
};

struct GetTriangles_Output_t
{
    CUtlVector<GetTriangles_MaterialBatch_t> m_MaterialBatches;
    float m_BoneToWorld[MAXSTUDIOBONES][3][4];
};

static_assert(sizeof(DrawModelInfo_t) == 0x20);
static_assert(offsetof(DrawModelInfo_t, m_pHardwareData) == 0x8);
static_assert(offsetof(DrawModelInfo_t, m_pClientEntity) == 0x10);
static_assert(offsetof(DrawModelInfo_t, m_Decals) == 0x18);
static_assert(offsetof(DrawModelInfo_t, m_Skin) == 0x1A);
static_assert(offsetof(DrawModelInfo_t, m_Lod) == 0x1B);
static_assert(offsetof(DrawModelInfo_t, m_Body) == 0x1C);
static_assert(offsetof(DrawModelInfo_t, m_HitboxSet) == 0x1E);

static_assert(sizeof(CUtlVectorFixed<IMaterial*, MAX_DRAW_MODEL_INFO_MATERIALS>) == 0x48);
static_assert(sizeof(CFastTimer) == 0x8);
static_assert(sizeof(DrawModelResults_t) == 0x70);
static_assert(offsetof(DrawModelResults_t, m_TextureMemoryBytes) == 0x4);
static_assert(offsetof(DrawModelResults_t, m_NumHardwareBones) == 0x8);
static_assert(offsetof(DrawModelResults_t, m_NumBatches) == 0xC);
static_assert(offsetof(DrawModelResults_t, m_NumMaterials) == 0x10);
static_assert(offsetof(DrawModelResults_t, m_nLODUsed) == 0x14);
static_assert(offsetof(DrawModelResults_t, m_flLODMetric) == 0x18);
static_assert(offsetof(DrawModelResults_t, m_RenderTime) == 0x20);
static_assert(offsetof(DrawModelResults_t, m_Materials) == 0x28);

static_assert(sizeof(GetTriangles_Vertex_t) == 0x54);
static_assert(offsetof(GetTriangles_Vertex_t, m_Normal) == 0xC);
static_assert(offsetof(GetTriangles_Vertex_t, m_TangentS) == 0x18);
static_assert(offsetof(GetTriangles_Vertex_t, m_TexCoord) == 0x28);
static_assert(offsetof(GetTriangles_Vertex_t, m_BoneWeight) == 0x30);
static_assert(offsetof(GetTriangles_Vertex_t, m_BoneIndex) == 0x40);
static_assert(offsetof(GetTriangles_Vertex_t, m_NumBones) == 0x50);

static_assert(sizeof(CUtlVector<GetTriangles_Vertex_t>) == 0x20);
static_assert(sizeof(CUtlVector<std::int32_t>) == 0x20);
static_assert(sizeof(GetTriangles_MaterialBatch_t) == 0x48);
static_assert(offsetof(GetTriangles_MaterialBatch_t, m_Verts) == 0x8);
static_assert(offsetof(GetTriangles_MaterialBatch_t, m_TriListIndices) == 0x28);
static_assert(sizeof(float[3][4]) == 0x30);
static_assert(sizeof(GetTriangles_Output_t) == 0x3020);
static_assert(offsetof(GetTriangles_Output_t, m_BoneToWorld) == 0x20);

static_assert(std::is_standard_layout_v<DrawModelInfo_t>);
static_assert(std::is_standard_layout_v<DrawModelResults_t>);
static_assert(std::is_standard_layout_v<GetTriangles_Vertex_t>);
static_assert(std::is_standard_layout_v<GetTriangles_MaterialBatch_t>);
static_assert(std::is_standard_layout_v<GetTriangles_Output_t>);

inline constexpr char STUDIO_RENDER_INTERFACE_VERSION[] = "VStudioRender026";

struct StudioRenderConfig_t
{
    std::uint32_t m_Opaque[8];
};

using StudioModelHandle_t = std::uint16_t;
using StudioRenderCallbackFn = void (*)();

enum class StudioRenderOverrideType : std::int32_t
{
    Normal = 0,
    BuildShadows = 1,
    DepthWrite = 2,
    SsaoDepthWrite = 3,
};

class IStudioRender : public IAppSystem
{
  public:
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void UpdateConfig(const StudioRenderConfig_t& config) = 0;
    virtual void GetCurrentConfig(StudioRenderConfig_t& config) = 0;
    virtual bool LoadModel(studiohdr_t* studioHdr, vertexFileHeader_t* vertexData, studiohwdata_t* hardwareData, std::uint32_t* loadFlags) = 0;
    virtual void UnloadModel(studiohwdata_t* hardwareData) = 0;
    virtual void RefreshStudioHdr(studiohdr_t* studioHdr, studiohwdata_t* hardwareData) = 0;
    virtual void SetEyeViewTarget(const studiohdr_t* studioHdr, int bodyIndex,
                                  const Vector3& worldPosition) = 0; // 15
    virtual void SetLocalLights(const LightDesc_t* lights, int count) = 0;
    virtual void SetLocalLight(int lightIndex, const LightDesc_t& light) = 0;
    virtual bool AddLocalLightToState(MaterialLightingState_t& lightingState, const LightDesc_t& light) = 0;
    virtual void SetMaterialLightingState(const MaterialLightingState_t& lightingState) = 0;
    virtual void ResetMaterialLightingState() = 0;
    virtual void SetViewState(const Vector3& origin, const Vector3& right, const Vector3& up,
                              const Vector3& planeNormal) = 0;
    virtual int GetNumLODs(const studiohwdata_t& hardwareData) const = 0;
    virtual float GetLODSwitchValue(const studiohwdata_t& hardwareData, int lod) const = 0;
    virtual void SetLODSwitchValue(studiohwdata_t& hardwareData, int lod, float switchValue) = 0;
    virtual void SetColorModulation(const float* color) = 0;
    virtual void SetAlphaModulation(float alpha, int mode) = 0;
    virtual void DrawModel(DrawModelResults_t* results, DrawModelInfo_t& info, const void* renderData, int unknown, int flags) = 0;
    virtual std::uintptr_t DrawModelStaticProp(const DrawModelInfo_t& info, int unknown0, const void* modelToWorld, int flags,
                                               bool unknown1) = 0; // 28
    virtual int GetStaticModelMeshList(const studiohdr_t* studioHdr, const studiohwdata_t* hardwareData, int skin, int lod,
                                       std::uint32_t materialFlags, void* meshRecords, int capacity) = 0;
    virtual void ForcedMaterialOverride(IMaterial* material,
                                        StudioRenderOverrideType overrideType = StudioRenderOverrideType::Normal) = 0;
    virtual void ClearForcedMaterialOverride() = 0;
    virtual StudioRenderOverrideType GetForcedMaterialOverrideType() const = 0;
    virtual void ClearAllDecals() = 0;
    virtual void DestroyDecalList(StudioDecalHandle_t decalList) = 0;
    virtual void AddDecal(StudioDecalHandle_t decalList, StudioModelHandle_t model, void* modelData, const Ray_t& ray, const Vector3& decalUp,
                          IMaterial* material, float radius, int body, bool noPokeThrough, int maxLODToDecal) = 0;
    virtual std::uintptr_t DrawDecalBatch(int count, void* records, void* decalHandles, void* drawData, void* state, int lod, int flags) = 0;
    virtual bool ValidateStaticPropDecalData(void* model, void* modelData, void* triangles, void* bounds, void* scratch, void* output) = 0;
    virtual void GetMaterialOverride(IMaterial** material, StudioRenderOverrideType* overrideType) = 0; // 38
    virtual void GetPerfStats(DrawModelResults_t* results, const DrawModelInfo_t& info,
                              void* spewBuffer) const = 0; // 39
    virtual void GetTriangles(const DrawModelInfo_t& info, const float (*boneToWorld)[3][4],
                              GetTriangles_Output_t& output) = 0;
    virtual int GetMaterialList(studiohdr_t* studioHdr, int capacity, IMaterial** materials) = 0;
    virtual int GetMaterialListFromBodyAndSkin(StudioModelHandle_t model, int skin, int body, int capacity, IMaterial** materials) = 0;
    virtual std::uintptr_t DrawModelArray(std::uint32_t count, const void* instances, int stride,
                                          int flags) = 0;
    virtual std::uintptr_t DrawStaticPropModelArray(std::uint32_t count, const void* instances, int stride, int flags) = 0;
    virtual void SetRenderCallback(StudioRenderCallbackFn callback) = 0;
    virtual void GetCurrentThreadId(std::uint32_t* threadId) = 0;
    virtual void NoOp() = 0;
    virtual void* GetRenderState() = 0;
};

static_assert(sizeof(StudioRenderConfig_t) == 0x20);
static_assert(alignof(StudioRenderConfig_t) == alignof(std::uint32_t));
static_assert(sizeof(StudioModelHandle_t) == 2);
static_assert(sizeof(StudioDecalHandle_t) == 2);
static_assert(sizeof(IStudioRender) == sizeof(void*));
static_assert(std::is_abstract_v<IStudioRender>);
