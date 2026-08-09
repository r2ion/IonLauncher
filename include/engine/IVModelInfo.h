#pragma once

#include "game/client/iclientrenderable.h"
#include <cstddef>
#include <cstdint>

class CUtlBuffer;
class KeyValues;
class IMaterial;
struct studiohdr_t;
struct studiohwdata_t;
struct model_t;
struct vcollide_t;
struct virtualmodel_t;
struct Vector3;


inline constexpr char VMODELINFO_CLIENT_INTERFACE_VERSION[] = "VModelInfoClient004";
inline constexpr char VMODELINFO_SERVER_INTERFACE_VERSION[] = "VModelInfoServer002";

class IVModelInfo
{
public:
	virtual ~IVModelInfo() = default;
	virtual const model_t* GetModel(int modelIndex) = 0;
	virtual int GetModelIndex(const char* pModelName) = 0;
	virtual const char* GetModelName(const model_t* pModel) = 0;
	virtual const char* GetModelNameByIndex(int modelIndex) = 0;
	virtual vcollide_t* GetVCollideByIndex(int modelIndex) = 0;
	virtual vcollide_t* GetVCollide(const model_t* pModel) = 0;
	virtual void* GetPhysicsGeometry(int modelIndex) = 0;
	virtual void GetModelBounds(const model_t* pModel, Vector3* pMins, Vector3* pMaxs) = 0;
	virtual void GetModelRenderBounds(const model_t* pModel, Vector3* pMins, Vector3* pMaxs) = 0;
	virtual int GetModelFrameCount(const model_t* pModel) = 0;
	virtual int GetModelType(const model_t* pModel) = 0;
	virtual void* GetModelExtraData(const model_t* pModel) = 0;
	virtual bool ModelHasMaterialProxy(const model_t* pModel) = 0;
	virtual bool IsTranslucent(const model_t* pModel) = 0;
	virtual bool IsTranslucentTwoPass(const model_t* pModel) = 0;
	virtual void RecomputeTranslucency(const model_t* pModel, int skin, int body,
		IClientRenderable* pRenderable, float alpha) = 0;
	virtual int GetModelMaterialCountForSkin(const model_t* pModel, int skin, int body) = 0;
	virtual int GetModelMaterialCount(const model_t* pModel) = 0;
	virtual void GetModelMaterials(const model_t* pModel, int count, IMaterial** ppMaterials) = 0;
	virtual bool IsModelVertexLit(const model_t* pModel) = 0;
	virtual const char* GetModelKeyValueText(const model_t* pModel) = 0;
	virtual bool GetModelKeyValue(const model_t* pModel, CUtlBuffer* pBuffer) = 0;
	virtual float GetModelRadius(const model_t* pModel) = 0;
	virtual const studiohdr_t* FindModelByCache(void* pCache) = 0;
	virtual const studiohdr_t* FindModel(const studiohdr_t* pStudioHeader, void** ppCache,
		const char* pModelName) = 0;
	virtual virtualmodel_t* GetVirtualModel(const studiohdr_t* pStudioHeader) = 0;
	virtual int GetModelContents(int modelIndex) = 0;
	virtual studiohdr_t* GetStudioModel(const model_t* pModel) = 0;
	virtual int GetModelSpriteWidth(const model_t* pModel) = 0;
	virtual int GetModelSpriteHeight(const model_t* pModel) = 0;
	virtual studiohwdata_t* GetModelHardwareData(const model_t* pModel) = 0;
	virtual bool IsUsingFBTexture(const model_t* pModel, int skin, int body,
		IClientRenderable* pRenderable) = 0;
	virtual std::uint16_t GetCacheHandle(const model_t* pModel) = 0;
	virtual int GetBrushModelPlaneCount(const model_t* pModel) = 0;
	virtual void OnLevelChange() = 0;
	virtual bool UsesEnvCubemap(const model_t* pModel) const = 0; // 36
	virtual KeyValues* GetModelKeyValues(model_t* pModel) = 0;
};

class IVModelInfoClient : public IVModelInfo
{
};

static_assert(sizeof(IVModelInfo) == sizeof(void*));
static_assert(sizeof(IVModelInfoClient) == sizeof(void*));
