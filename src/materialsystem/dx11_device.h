#pragma once

#include "tier0/module.h"

#include <d3d11.h>

class CDx11Device final
{
public:
	struct Snapshot
	{
		ID3D11Device* m_pDevice = nullptr;
		ID3D11DeviceContext* m_pContext = nullptr;

		explicit operator bool() const noexcept
		{
			return m_pDevice && m_pContext;
		}
	};

	static Snapshot GetSnapshot() noexcept
	{
		return {s_ppDevice ? *s_ppDevice : nullptr, s_ppImmediateContext ? *s_ppImmediateContext : nullptr};
	}

	static void Initialize(CModule module);

private:
	inline static ID3D11Device** s_ppDevice = nullptr;
	inline static ID3D11DeviceContext** s_ppImmediateContext = nullptr;
};
