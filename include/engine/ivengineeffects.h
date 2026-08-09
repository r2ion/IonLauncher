#pragma once

#include "dlight.h"
#include "materialsystem/imaterial.h"

#include <cstdint>

struct model_t;

inline constexpr char VENGINE_EFFECTS_INTERFACE_VERSION[] = "VEngineEffects001";

class IVEfx
{
public:
	virtual void PlayerDecalShoot(IMaterial* pMaterial, std::uint32_t entityIndex, const model_t* pModel,
		const Vector3& modelOrigin, const QAngle& modelAngles, const Vector3& position,
		const Vector3* pDecalAxis, int flags, float scale, const Vector3& normal) = 0;
	virtual void FlushQueuedDecals() = 0;
	virtual dlight_t* AllocDlight(std::uint32_t key) = 0;
	virtual void FreeDlightByKey(int key) = 0;
	virtual ~IVEfx() = default;
};
