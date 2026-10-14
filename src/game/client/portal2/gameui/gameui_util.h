#pragma once

#include "tier1/convar.h"

#include <cstddef>

class CGameUIConVarRef
{
public:
	struct cv_t
	{
		IConVar* m_pConVar;
		ConVar* m_pConVarState;
	};
	cv_t m_Info[1];

	CGameUIConVarRef* Init(const char* convarName);
};

static_assert(sizeof(CGameUIConVarRef) == 0x10);
static_assert(alignof(CGameUIConVarRef) == 0x8);
static_assert(offsetof(CGameUIConVarRef, m_Info) == 0x0);
static_assert(offsetof(CGameUIConVarRef::cv_t, m_pConVarState) == 0x8);
