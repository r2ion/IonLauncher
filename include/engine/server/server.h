#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/client/client.h"
#include "engine/networkstringtable.h"
#include "inetchannel.h"
#include "tier1/utlmemory.h"

enum server_state_t : std::int32_t
{
    ss_dead = 0,
    ss_loading,
    ss_active,
    ss_paused,
};

class CServer : public IConnectionlessPacketHandler
{
  public:
    server_state_t GetState() const
    {
        return m_State;
    }
    int GetTick() const
    {
        return m_nTickCount;
    }
    bool CanApplyOverlays() const
    {
        return m_bApplyOverlays;
    }

    const char* GetMapName() const
    {
        return m_szMapname;
    }
    const char* GetMapGroupName() const
    {
        return m_szMapGroupName;
    }

    int GetNumClasses() const
    {
        return m_nServerClasses;
    }
    int GetClassBits() const
    {
        return m_nServerClassBits;
    }
    int GetSpawnCount() const
    {
        return m_nSpawnCount;
    }
    int GetMaxClients() const
    {
        return m_nMaxClients;
    }

    CClient* GetClient(int index)
    {
        return &m_Clients[index];
    }
    CClientExtended* GetClientExtended(int index)
    {
        return &sm_ClientsExtended[index];
    }

    float GetTime() const
    {
        return m_nTickCount * m_flTickInterval;
    }
    float GetCPUUsage() const
    {
        return m_fCPUPercent;
    }

    bool IsActive() const
    {
        return m_State >= ss_active;
    }
    bool IsLoading() const
    {
        return m_State == ss_loading;
    }
    bool IsDedicated() const
    {
        return m_bIsDedicated;
    }

  private:
    server_state_t m_State;                        // 0x0008
    std::int32_t m_Socket;                         // 0x000C
    std::int32_t m_nTickCount;                     // 0x0010
    bool m_bResetMaxTeams;                         // 0x0014
    char m_szMapname[64];                          // 0x0015
    char m_szMapGroupName[64];                     // 0x0055
    char m_szPassword[32];                         // 0x0095
    std::byte m_Reserved00B5[3];                   // 0x00B5
    bool m_bUnknown00B8;                           // 0x00B8
    std::byte m_Reserved00B9[3];                   // 0x00B9
    std::uint32_t m_WorldMapCRC;                   // 0x00BC
    std::uint32_t m_ClientDllCRC;                  // 0x00C0
    std::byte m_Padding00C4[4];                    // 0x00C4
    CNetworkStringTableContainer* m_StringTables;  // 0x00C8
    CNetworkStringTable* m_pInstanceBaselineTable; // 0x00D0
    CNetworkStringTable* m_pLightStyleTable;       // 0x00D8
    CNetworkStringTable* m_pUserInfoTable;         // 0x00E0
    CNetworkStringTable* m_pServerQueryTable;      // 0x00E8
    bool m_bApplyOverlays;                         // 0x00F0
    bool m_bUpdateFrame;                           // 0x00F1
    bool m_bUseReputation;                         // 0x00F2
    bool m_bSimulating;                            // 0x00F3
    std::byte m_Padding00F4[4];                    // 0x00F4
    bf_write m_Signon;                             // 0x00F8
    CUtlMemory<std::uint8_t> m_SignonBuffer;       // 0x0118
    std::int32_t m_nServerClasses;                 // 0x0130
    std::int32_t m_nServerClassBits;               // 0x0134
    char m_szHostInfo[128];                        // 0x0138
    std::byte m_Reserved01B8[16];                  // 0x01B8
    float m_flUnknown01C8;                         // 0x01C8
    std::byte m_Reserved01CC[112];                 // 0x01CC
    float m_flStartTime;                           // 0x023C
    std::int32_t m_nMaxClients;                    // 0x0240
    std::int32_t m_nSpawnCount;                    // 0x0244
    float m_flTickInterval;                        // 0x0248
    float m_flTimescale;                           // 0x024C
    CClient m_Clients[MAX_PLAYERS];                // 0x0250
    std::byte m_Reserved5AE750[8];                 // 0x5AE750
    bool m_bIsDedicated;                           // 0x5AE758
    std::byte m_Reserved5AE759[39];                // 0x5AE759
    float m_fCPUPercent;                           // 0x5AE780
    float m_fStartTime;                            // 0x5AE784
    float m_fLastCPUCheckTime;                     // 0x5AE788
    bool m_bTeams[MAX_TEAMS];                      // 0x5AE78C

    static CClientExtended sm_ClientsExtended[MAX_PLAYERS];
};

static_assert(sizeof(CServer) == 0x5AE7B0);

extern CServer* g_pServer;
