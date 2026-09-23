#include "server/recipientfilter.h"

#include "tier0/module.h"

using AddAllPlayers_t = void(*)(CRecipientFilter* filter);
using AddRecipient_t = void(*)(CRecipientFilter* filter, const CPlayer* player);
using RemoveRecipient_t = void(*)(CRecipientFilter* filter, const CPlayer* player);
using RemoveRecipientByPlayerIndex_t = void(*)(CRecipientFilter* filter, int playerIndex);

AddAllPlayers_t CRecipientFilter__AddAllPlayers;
AddRecipient_t CRecipientFilter__AddRecipient;
RemoveRecipient_t CRecipientFilter__RemoveRecipient;
RemoveRecipientByPlayerIndex_t CRecipientFilter__RemoveRecipientByPlayerIndex;

CRecipientFilter::CRecipientFilter()
{
    Reset();
}

void CRecipientFilter::Reset()
{
    m_bReliable = false;
    m_bInitMessage = false;
    m_Recipients.RemoveAll();
    m_bUsingPredictionRules = false;
    m_bIgnorePredictionCull = false;
}

bool CRecipientFilter::IsReliable() const
{
    return m_bReliable;
}

void CRecipientFilter::MakeReliable()
{
    m_bReliable = true;
}

bool CRecipientFilter::IsInitMessage() const
{
    return m_bInitMessage;
}

void CRecipientFilter::MakeInitMessage()
{
    m_bInitMessage = true;
}

int CRecipientFilter::GetRecipientCount() const
{
    return m_Recipients.Count();
}

int CRecipientFilter::GetRecipientIndex(const int slot) const
{
    return m_Recipients.IsValidIndex(slot) ? m_Recipients[slot].m_nIndex : -1;
}

bool CRecipientFilter::IsReplayMessage(const int slot) const
{
    return m_Recipients.IsValidIndex(slot) && m_Recipients[slot].m_bIsReplayMessage;
}

int CRecipientFilter::FindSlotForIndex(const int index) const
{
    FOR_EACH_VEC(m_Recipients, slot)
    {
        if (m_Recipients[slot].m_nIndex == index)
            return slot;
    }

    return m_Recipients.InvalidIndex();
}

void CRecipientFilter::AddAllPlayers()
{
    CRecipientFilter__AddAllPlayers(this);
}

void CRecipientFilter::AddRecipient(const CPlayer* player)
{
    if (player)
        CRecipientFilter__AddRecipient(this, player);
}

void CRecipientFilter::RemoveAllRecipients()
{
    m_Recipients.RemoveAll();
}

void CRecipientFilter::RemoveRecipient(const CPlayer* player)
{
    CRecipientFilter__RemoveRecipient(this, player);
}

void CRecipientFilter::RemoveRecipientByPlayerIndex(const int playerIndex)
{
    CRecipientFilter__RemoveRecipientByPlayerIndex(this, playerIndex);
}

ON_DLL_LOAD("server.dll", ServerRecipientFilter, [](CModule module)
{
    CRecipientFilter__AddAllPlayers = module.Offset(0x1E9940).RCast<AddAllPlayers_t>();
    CRecipientFilter__AddRecipient = module.Offset(0x1E9B30).RCast<AddRecipient_t>();
    CRecipientFilter__RemoveRecipient = module.Offset(0x1EA800).RCast<RemoveRecipient_t>();
    CRecipientFilter__RemoveRecipientByPlayerIndex = module.Offset(0x1EA820).RCast<RemoveRecipientByPlayerIndex_t>();
})
