#include "cliententitylist.h"
#include "client/icliententity.h"
#include "core/tier1.h"
#include "engine/shared/maxplayers.h"

DECLARE_MODULE(ClientEntityListHooks)

IClientEntityList* g_pClientEntityList;

IHandleEntity* CBaseHandle::Get() const
{
    return g_pClientEntityList->GetClientEntityFromHandle(*this);
}

const CBaseHandle& CBaseHandle::Set(const IHandleEntity* entity)
{
    m_Index = entity ? entity->GetRefEHandle().ToInt() : 0xFFFFFFFFu;
    return *this;
}

bool CBaseHandle::operator<(const IHandleEntity* entity) const
{
    return m_Index < (entity ? entity->GetRefEHandle().ToInt() : 0xFFFFFFFFu);
}

DECLARE_HOOK(IsPlayerMutedOrBlockedByClientNum, client.dll + 0x55BF90, [](auto& hook, int nClientNum) -> bool
{
    if (nClientNum < 0 || nClientNum >= GetMaxPlayers())
        return false;

    return hook.Original(nClientNum);
})

ON_DLL_LOAD("client.dll", ClientEntityList, [](CModule module)
{
    DISPATCH_MODULE(ClientEntityListHooks)
    g_pClientEntityList = Sys_GetFactoryPtr("client.dll", VCLIENTENTITYLIST_INTERFACE_VERSION).RCast<IClientEntityList*>();
})
