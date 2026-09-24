#pragma once

#include <d3d11.h>

inline ID3D11Device** g_ppGameDevice = nullptr;
inline ID3D11DeviceContext** g_ppImmediateContext = nullptr;
inline IDXGISwapChain** g_ppSwapChain = nullptr;

inline ID3D11Device* D3D11Device()
{
    return g_ppGameDevice ? *g_ppGameDevice : nullptr;
}

inline ID3D11DeviceContext* D3D11DeviceContext()
{
    return g_ppImmediateContext ? *g_ppImmediateContext : nullptr;
}

inline IDXGISwapChain* DXGISwapChain()
{
    return g_ppSwapChain ? *g_ppSwapChain : nullptr;
}
