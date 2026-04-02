#pragma once

class IClientEntityList
{
public:
	virtual ~IClientEntityList();
	virtual int GetClientEntityFromHandle(int hEnt);
	virtual CBaseEntity* GetClientEntity(int entnum);
	virtual CBaseEntity* GetClientEntityFromHandle(int* handle);
};

extern IClientEntityList* g_pClientEntityList;


