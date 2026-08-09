#pragma once

class IMaterialSystem;
class IMaterialSystemHardwareConfig;

inline constexpr char CLIENTRENDERTARGETS_INTERFACE_VERSION[] = "ClientRenderTargets001";

class IClientRenderTargets
{
public:
	virtual void InitClientRenderTargets(IMaterialSystem* pMaterialSystem,
		IMaterialSystemHardwareConfig* pHardwareConfig) = 0;
	virtual void ShutdownClientRenderTargets() = 0;
};

static_assert(sizeof(IClientRenderTargets) == sizeof(void*));
