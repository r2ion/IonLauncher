#include "materialsystem/dx11_device.h"

void CDx11Device::Initialize(CModule module)
{
	s_ppDevice = module.Offset(0x14E8DD0).RCast<ID3D11Device**>();
	s_ppImmediateContext = module.Offset(0x14E8DD8).RCast<ID3D11DeviceContext**>();
}

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", Dx11Device, [](CModule module) { CDx11Device::Initialize(module); })
