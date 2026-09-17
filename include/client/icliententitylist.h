#pragma once

#include "engine/basehandle.h"
#include <cstdint>

using ClientEntityHandle_t = std::uint32_t;

inline constexpr char VCLIENTENTITYLIST_INTERFACE_VERSION[] = "VClientEntityList003";

class IClientEntity;
class IClientNetworkable;
class IClientUnknown;

class IClientEntityList
{
  public:
    virtual IClientNetworkable* GetClientNetworkable(int entityNumber) = 0; // 0
    virtual IClientNetworkable* GetClientNetworkableFromHandle(CBaseHandle entityHandle) = 0;
    virtual IClientUnknown* GetClientUnknownFromHandle(CBaseHandle entityHandle) = 0;
    virtual IClientEntity* GetClientEntity(int entityNumber) = 0;
    virtual IClientEntity* GetClientEntityFromHandle(CBaseHandle entityHandle) = 0; // 4
    virtual int NumberOfEntities(bool includeNonNetworkable = false) = 0;
    virtual int GetHighestEntityIndex() = 0;
    virtual void SetMaxEntities(int maxEntities) = 0;
    virtual int GetMaxEntities() = 0;
};

static_assert(sizeof(IClientEntityList) == sizeof(void*));
