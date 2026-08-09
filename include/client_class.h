#pragma once

#include <cstddef>

class CRecvTable;
class IClientNetworkable;

using CreateClientClassFn = IClientNetworkable* (*)(int entityNumber, int serialNumber);
using CreateEventFn = IClientNetworkable* (*)();

class ClientClass
{
public:
	const char* GetName() const
	{
		return m_pNetworkName;
	}

public:
	CreateClientClassFn m_pCreateFn;
	CreateEventFn m_pCreateEventFn;
	char* m_pNetworkName;
	CRecvTable* m_pRecvTable;
	ClientClass* m_pNext;
	int m_ClassID;
	int m_ClassSize;
};

static_assert(sizeof(ClientClass) == 0x30);
static_assert(offsetof(ClientClass, m_pNetworkName) == 0x10);
static_assert(offsetof(ClientClass, m_pRecvTable) == 0x18);
