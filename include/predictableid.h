//========= Copyright Valve Corporation, All rights reserved. ============//
#pragma once

#include <cstring>
#include "tier0/platform.h"

class C_PredictableId
{
  public:
    C_PredictableId() : m_PredictableID{} {}

    static void ResetInstanceCounters();
    bool IsActive() const { return GetRaw() != 0; }
    void Init(int player, int command, const char* classname, const char* module, int line);

    int GetPlayer() const { return m_PredictableID.player; }
    int GetHash() const { return m_PredictableID.hash; }
    int GetInstanceNumber() const { return m_PredictableID.instance; }
    int GetCommandNumber() const { return m_PredictableID.command; }

    void SetAcknowledged(bool ack) { m_PredictableID.ack = ack; }
    bool GetAcknowledged() const { return m_PredictableID.ack != 0; }

    int GetRaw() const
    {
        int raw;
        std::memcpy(&raw, &m_PredictableID, sizeof(raw));
        return raw;
    }
    void SetRaw(int raw) { std::memcpy(&m_PredictableID, &raw, sizeof(raw)); }
    const char* Describe() const;

    bool operator==(const C_PredictableId& other) const { return (GetRaw() & ~1u) == (other.GetRaw() & ~1u); }
    bool operator!=(const C_PredictableId& other) const { return !(*this == other); }

    struct bitfields
    {
        unsigned int ack : 1;
        unsigned int player : 5;
        unsigned int command : 10;
        unsigned int hash : 12;
        unsigned int instance : 4;
    };

    bitfields m_PredictableID;

  private:
    void SetCommandNumber(int commandNumber) { m_PredictableID.command = commandNumber; }
    void SetPlayer(int playerIndex) { m_PredictableID.player = playerIndex; }
    void SetInstanceNumber(int counter) { m_PredictableID.instance = counter; }
};

FORCEINLINE void NetworkVarConstruct(C_PredictableId&) {}

static_assert(sizeof(C_PredictableId) == 0x4);
static_assert(alignof(C_PredictableId) == 0x4);
