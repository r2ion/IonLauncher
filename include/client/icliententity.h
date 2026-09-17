#pragma once

#include "client/iclientunknown.h"
#include "client/iclientrenderable.h"
#include "client/iclientnetworkable.h"
#include "client/iclientthinkable.h"

class IClientEntity : public IClientUnknown, public IClientRenderable, public IClientNetworkable, public IClientThinkable
{
public:
	virtual const Vector3D& GetAbsOrigin() const = 0;
	virtual const QAngle& GetAbsAngles() const = 0;
	virtual bool IsBlurred() = 0; // 10
	virtual const char* GetModelIndexName() const = 0; // 11
	virtual const char* EngineGetDebugName() = 0;
};

static_assert(sizeof(IClientEntity) == 0x28);
