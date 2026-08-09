#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

struct model_t;

enum class ModelReloadType_t : std::uint32_t
{
	LodChanged = 0,
	Everything,
	RefreshModels,
	Async,
};

class IModelLoader
{
public:
	enum REFERENCETYPE : std::uint32_t
	{
		FMODELLOADER_LOADED = 1u << 0,
		FMODELLOADER_SERVER = 1u << 1,
		FMODELLOADER_CLIENT = 1u << 2,
		FMODELLOADER_CLIENTDLL = 1u << 3,
		FMODELLOADER_STATICPROP = 1u << 4,
		FMODELLOADER_DETAILPROP = 1u << 5,
		FMODELLOADER_SIMPLEWORLD = 1u << 6,
		FMODELLOADER_DYNSERVER = 1u << 7,
		FMODELLOADER_DYNCLIENT = 1u << 8,
		FMODELLOADER_COMBINED = 1u << 9,
		FMODELLOADER_TOUCHED_BY_PRELOAD = 1u << 15,
		FMODELLOADER_LOADED_BY_PRELOAD = 1u << 16,
		FMODELLOADER_TOUCHED_MATERIALS = 1u << 17,
	};

	virtual void ShutdownModelLoader() = 0;
	virtual void InitModelLoader() = 0;
	virtual void Extension002() = 0;
	virtual void Extension003() = 0;
	virtual void Extension004() = 0;
	virtual void Extension005() = 0;
	virtual void Extension006() = 0;
	virtual void Extension007() = 0;
	virtual void Extension008() = 0;
	virtual void Extension009() = 0;
	virtual void ClearServerReference(model_t* model) = 0;
	virtual void UnreferenceAllModels() = 0;
	virtual void UnloadUnreferencedModels() = 0;
	virtual void Extension013() = 0;
	virtual void Extension014() = 0;
	virtual void Extension015() = 0;
	virtual void Extension016() = 0;
	virtual void Extension017() = 0;
	virtual void Extension018() = 0;
	virtual void ReloadModels(ModelReloadType_t reloadType) = 0;
	virtual void ReloadModel(const char* modelPath) = 0;
	virtual void Extension021() = 0;
	virtual void FlushModelCaches(ModelReloadType_t reloadType) = 0;
	virtual void RetouchModels(ModelReloadType_t reloadType) = 0;
	virtual void FlushModelByName(const char* modelPath) = 0;
	virtual bool IsModelLoaded(const model_t* model) = 0;
	virtual void Extension026() = 0;
	virtual void Extension027() = 0;
	virtual void Extension028() = 0;
};

class CModelLoader : public IModelLoader
{
public:
	void ShutdownModelLoader() override;
	void InitModelLoader() override;
	void Extension002() override;
	void Extension003() override;
	void Extension004() override;
	void Extension005() override;
	void Extension006() override;
	void Extension007() override;
	void Extension008() override;
	void Extension009() override;
	void ClearServerReference(model_t* model) override;
	void UnreferenceAllModels() override;
	void UnloadUnreferencedModels() override;
	void Extension013() override;
	void Extension014() override;
	void Extension015() override;
	void Extension016() override;
	void Extension017() override;
	void Extension018() override;
	void ReloadModels(ModelReloadType_t reloadType) override;
	void ReloadModel(const char* modelPath) override;
	void Extension021() override;
	void FlushModelCaches(ModelReloadType_t reloadType) override;
	void RetouchModels(ModelReloadType_t reloadType) override;
	void FlushModelByName(const char* modelPath) override;
	bool IsModelLoaded(const model_t* model) override;
	void Extension026() override;
	void Extension027() override;
	void Extension028() override;

private:
	std::byte m_Unknown0008[0x2C8];
};


static_assert(sizeof(IModelLoader::REFERENCETYPE) == sizeof(std::uint32_t));
static_assert(static_cast<std::uint32_t>(ModelReloadType_t::RefreshModels) == 2);
static_assert(sizeof(IModelLoader) == 0x8);
static_assert(std::is_abstract_v<IModelLoader>);
static_assert(std::is_polymorphic_v<IModelLoader>);
static_assert(std::is_base_of_v<IModelLoader, CModelLoader>);
static_assert(std::is_polymorphic_v<CModelLoader>);
static_assert(sizeof(CModelLoader) == 0x2D0);
