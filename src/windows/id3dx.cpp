#include "windows/id3dx.h"

#include "tier0/module.h"

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", D3D11, [](CModule module)
{
    g_ppGameDevice = module.Offset(0x14E8DD0).RCast<ID3D11Device**>();
    g_ppImmediateContext = module.Offset(0x14E8DD8).RCast<ID3D11DeviceContext**>();
})
