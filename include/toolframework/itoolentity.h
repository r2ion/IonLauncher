#pragma once

#include "game/client/iclientrenderable.h"
#include "interface.h"
#include "mathlib/vector.h"

#include <cstdint>
#include <cstddef>

class Color;
class CBaseEntity;
class IClientEntity;
class IServerEntity;

enum RenderableModelType_t : int;
enum RenderableTranslucencyType_t : int;

using EntitySearchResult = void*;
using HTOOLHANDLE = std::uint32_t;

inline constexpr HTOOLHANDLE HTOOLHANDLE_INVALID = 0;
inline constexpr char VCLIENTTOOLS_INTERFACE_VERSION[] = "VCLIENTTOOLS001";
inline constexpr char VSERVERTOOLS_INTERFACE_VERSION[] = "VSERVERTOOLS001";

class IClientTools : public IBaseInterface
{
public:
	virtual HTOOLHANDLE AttachToEntity(EntitySearchResult entity) = 0;
	virtual void DetachFromEntity(EntitySearchResult entity) = 0;
	virtual EntitySearchResult GetEntity(HTOOLHANDLE handle) = 0;
	virtual bool IsValidHandle(HTOOLHANDLE handle) = 0;
	virtual int GetNumRecordables() = 0;
	virtual HTOOLHANDLE GetRecordable(int index) = 0;
	virtual EntitySearchResult NextEntity(EntitySearchResult currentEntity) = 0;
	virtual void SetEnabled(HTOOLHANDLE handle, bool enabled) = 0;
	virtual void SetRecording(HTOOLHANDLE handle, bool recording) = 0;
	virtual bool ShouldRecord(HTOOLHANDLE handle) = 0;
	virtual HTOOLHANDLE GetToolHandleForEntityByIndex(int entityIndex) = 0;
	virtual int GetModelIndex(HTOOLHANDLE handle) = 0;
	virtual const char* GetModelName(HTOOLHANDLE handle) = 0;
	virtual const char* GetClassname(HTOOLHANDLE handle) = 0;
	virtual void AddClientRenderable(IClientRenderable* pRenderable, bool drawWithViewModels,
		RenderableTranslucencyType_t translucencyType, RenderableModelType_t modelType) = 0;
	virtual void RemoveClientRenderable(IClientRenderable* pRenderable) = 0;
	virtual void MarkClientRenderableDirty(IClientRenderable* pRenderable) = 0;
	virtual bool DrawSprite(IClientRenderable* pRenderable, float scale, float frame, int renderMode, int renderFx,
		const Color& color, float proxyRadius, int* pVisibilityHandle) = 0;
	virtual EntitySearchResult GetLocalPlayer() = 0;
	virtual bool GetLocalPlayerEyePosition(Vector3& origin, QAngle& angles, float& fov) = 0;
	virtual int GetOwningWeaponEntIndex(int entityIndex) = 0;
	virtual int GetEntIndex(EntitySearchResult entity) = 0;
	virtual EntitySearchResult GetOwnerEntity(EntitySearchResult entity) = 0;
	virtual bool IsPlayer(EntitySearchResult entity) = 0;
	virtual bool IsCombatCharacter(EntitySearchResult entity) = 0;
	virtual bool IsNPC(EntitySearchResult entity) = 0;
	virtual bool IsRagdoll(EntitySearchResult entity) = 0;
	virtual bool IsViewModel(EntitySearchResult entity) = 0;
	virtual bool IsViewModelOrAttachment(EntitySearchResult entity) = 0;
	virtual bool IsWeapon(EntitySearchResult entity) = 0;
	virtual bool IsSprite(EntitySearchResult entity) = 0;
	virtual bool IsProp(EntitySearchResult entity) = 0;
	virtual bool IsBrush(EntitySearchResult entity) = 0;
	virtual Vector3 GetAbsOrigin(HTOOLHANDLE handle) = 0;
	virtual QAngle GetAbsAngles(HTOOLHANDLE handle) = 0;
	virtual void ReloadParticleDefinitions(const char* pFileName, const void* pBufferData, int length) = 0;
	virtual void EnableParticleSystems(bool enable) = 0;
	virtual bool IsRenderingThirdPerson() const = 0;
};

struct CEntityRespawnInfo
{
	int m_HammerId;
	std::uint32_t m_Pad0004;
	const char* m_EntityText;
};

class IServerTools : public IBaseInterface
{
public:
	virtual IServerEntity* GetIServerEntity(IClientEntity* pClientEntity) = 0;
	virtual bool SnapPlayerToPosition(const Vector3& origin, const QAngle& angles,
		IClientEntity* pClientPlayer = nullptr) = 0;
	virtual bool GetPlayerPosition(Vector3& origin, QAngle& angles,
		IClientEntity* pClientPlayer = nullptr) = 0;
	virtual bool IsInNoClipMode(IClientEntity* pClientPlayer = nullptr) = 0;
	virtual CBaseEntity* FirstEntity() = 0;
	virtual CBaseEntity* NextEntity(CBaseEntity* pEntity) = 0;
	virtual CBaseEntity* FindEntityByHammerID(int hammerId) = 0;
	virtual bool GetKeyValue(CBaseEntity* pEntity, const char* pField, char* pValue,
		std::uint32_t valueCapacity) = 0;
	virtual bool SetKeyValue(CBaseEntity* pEntity, const char* pField, const char* pValue) = 0;
	virtual bool SetKeyValue(CBaseEntity* pEntity, const char* pField, float value) = 0;
	virtual bool SetKeyValue(CBaseEntity* pEntity, const char* pField, const Vector3& value) = 0;
	virtual CBaseEntity* CreateEntityByName(const char* pClassName) = 0;
	virtual void DispatchSpawn(CBaseEntity* pEntity) = 0;
	virtual bool DestroyEntityByHammerId(int hammerId) = 0;
	virtual bool RespawnEntitiesWithEdits(const CEntityRespawnInfo* pInfos, int infoCount) = 0;
	virtual void ReloadParticleDefinitions(const char* pFileName, const void* pBufferData,
		int length) = 0;
	virtual void MoveEngineViewTo(const Vector3& position, const QAngle& angles) = 0;
	virtual void RemoveEntity(int hammerId) = 0;
};

static_assert(sizeof(IClientTools) == sizeof(void*));
static_assert(sizeof(CEntityRespawnInfo) == 0x10);
static_assert(offsetof(CEntityRespawnInfo, m_EntityText) == 0x8);
static_assert(sizeof(IServerTools) == sizeof(void*));
