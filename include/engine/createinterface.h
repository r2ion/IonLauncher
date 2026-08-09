#pragma once

#include "interface.h"

#include <cstddef>

using InterfaceFactoryFn = CreateInterfaceFn;

class InterfaceReg
{
public:
	InterfaceReg(InstantiateInterfaceFn fn, const char* pName);

	InstantiateInterfaceFn m_CreateFn;
	const char* m_pName;
	InterfaceReg* m_pNext;
};

static_assert(sizeof(InterfaceReg) == 0x18);
static_assert(alignof(InterfaceReg) == 0x8);
static_assert(offsetof(InterfaceReg, m_CreateFn) == 0x0);
static_assert(offsetof(InterfaceReg, m_pName) == 0x8);
static_assert(offsetof(InterfaceReg, m_pNext) == 0x10);
