#pragma once

#include <cstdint>

using ClientEntityHandle_t = std::uint32_t;

inline constexpr char VCLIENTENTITYLIST_INTERFACE_VERSION[] = "VClientEntityList003";

class IClientEntityList
{
public:
	virtual ~IClientEntityList() = default;
	virtual void* GetClientNetworkable(int entityNumber) = 0;
	virtual void* GetClientNetworkableFromHandle(ClientEntityHandle_t entityHandle) = 0;
	virtual void* GetClientUnknownFromHandle(ClientEntityHandle_t entityHandle) = 0;
	virtual void* GetClientEntity(int entityNumber) = 0;
	virtual void* GetClientEntityFromHandle(ClientEntityHandle_t entityHandle) = 0;
	virtual int NumberOfEntities(bool includeNonNetworkable) = 0;
	virtual int GetHighestEntityIndex() = 0;
	virtual void SetMaxEntities(int maxEntities) = 0;
	virtual int GetMaxEntities() = 0;
};

static_assert(sizeof(IClientEntityList) == sizeof(void*));
