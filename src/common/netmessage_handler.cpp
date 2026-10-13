#include "common/netmessages.h"

#include "engine/client/clientstate.h"
#include "engine/net_chan.h"
#include "tier0/vanilla.h"

#include <memory>

CNetMessageHandler g_NetMessageHandlerStorage;
CNetMessageHandler* g_NetMessageHandler = &g_NetMessageHandlerStorage;

bool CNetMessageHandler::RegisterMessages(CNetChan* const channel, INetMessageHandler* const handler,
                                          const std::vector<NetMessageFactory_t>& factories) const
{
    for (const NetMessageFactory_t factory : factories)
    {
        auto message = std::unique_ptr<CNetMessage>(factory(handler));
        if (!channel->RegisterMessage(message.get()))
            return false;

        message.release();
    }
    return true;
}

bool CNetMessageHandler::RegisterMessagesCLC(CNetChan* const channel, IClientMessageHandler* const handler) const
{
    if (g_pVanillaCompatibility->GetVanillaCompatibility())
        return true;

    return RegisterMessages(channel, handler, m_NETMessageFactories) && RegisterMessages(channel, handler, m_CLCMessageFactories);
}

bool CNetMessageHandler::RegisterMessagesSVC(CNetChan* const channel, IServerMessageHandler* const handler) const
{
    if (g_pVanillaCompatibility->GetVanillaCompatibility())
        return true;

    return RegisterMessages(channel, handler, m_NETMessageFactories) && RegisterMessages(channel, handler, m_SVCMessageFactories);
}
