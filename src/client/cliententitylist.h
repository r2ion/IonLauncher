#pragma once

class IClientEntityList
{
public:
	
	M_VMETHOD(CBaseEntity*, GetClientEntity, 3, (int hEnt), (this, hEnt))
};


extern IClientEntityList* g_pClientEntityList;


