#pragma once

#include "irecipientfilter.h"
#include "tier1/utlvector.h"

class CPlayer;

class CRecipientFilter : public IRecipientFilter
{
    struct Recipient_t
    {
        int m_nIndex;
        bool m_bIsReplayMessage;
    };

  public:
    CRecipientFilter();
    ~CRecipientFilter() override = default;

    CRecipientFilter(const CRecipientFilter&) = delete;
    CRecipientFilter& operator=(const CRecipientFilter&) = delete;

    bool IsReliable() const override;
    void MakeReliable() override;
    bool IsInitMessage() const override;
    int GetRecipientCount() const override;
    int GetRecipientIndex(int slot) const override;
    bool IsReplayMessage(int slot) const override;

    void Reset();
    void MakeInitMessage();
    int FindSlotForIndex(int index) const;

    void AddAllPlayers();
    void AddRecipient(const CPlayer* player);
    void RemoveAllRecipients();
    void RemoveRecipient(const CPlayer* player);
    void RemoveRecipientByPlayerIndex(int playerIndex);

  private:
    bool m_bReliable;
    bool m_bInitMessage;
    CUtlVector<Recipient_t> m_Recipients;
    bool m_bUsingPredictionRules;
    bool m_bIgnorePredictionCull;
};

static_assert(sizeof(CRecipientFilter) == 0x38);

class CSingleUserRecipientFilter : public CRecipientFilter
{
  public:
    explicit CSingleUserRecipientFilter(const CPlayer* player)
    {
        AddRecipient(player);
    }
};

class CBroadcastRecipientFilter : public CRecipientFilter
{
  public:
    CBroadcastRecipientFilter()
    {
        AddAllPlayers();
    }
};

class CReliableBroadcastRecipientFilter : public CBroadcastRecipientFilter
{
  public:
    CReliableBroadcastRecipientFilter()
    {
        MakeReliable();
    }
};
