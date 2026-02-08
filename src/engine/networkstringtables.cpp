#include "core/tier0.h"
#include "tier0/module.h"
#include "logging/logging.h"
#include "engine/bitbuf.h"
#include "core/convar/convar.h"
#include <cstring>
#include <intrin.h>
#include <mutex>
#include <vector>
#include <unordered_set>
AUTOHOOK_INIT()

// CUtlVector template
template<typename T>
struct CUtlVector
{
    T* m_pMemory;              // 0x00
    int m_nAllocationCount;    // 0x08
    int m_nGrowSize;           // 0x0C
    int m_Size;                // 0x10
    T* m_pElements;            // 0x14
};

// Forward declarations
struct CNetworkStringTable
{
    virtual ~CNetworkStringTable() = 0;
    virtual const char* GetTableName() = 0;
    virtual int GetTableId() = 0;
    virtual int GetNumStrings() = 0;
    virtual int GetMaxStrings() = 0;
};

// Based on IDA structures (x64 layout)
struct CNetworkStringTableContainer
{
    // 0x00: vtable
    // 0x08: padding
    // 0x10: m_bAllowCreation + m_nTickCount
    // 0x18: m_bLocked + m_bEnableRollback + padding
    CUtlVector<CNetworkStringTable*> m_Tables;  // 0x20, so m_Size is at 0x20 + 0x10 = 0x30

    virtual ~CNetworkStringTableContainer() = 0;
    virtual CNetworkStringTable* CreateStringTable(const char*, int, int, int, int) = 0;
    virtual void RemoveAllTables() = 0;
    virtual CNetworkStringTable* FindTable(const char*) = 0;
    virtual CNetworkStringTable* GetTable(int) = 0;
    virtual int GetNumTables() = 0;
};

// Hook WriteBaselines to instrument what's happening
// CNetworkStringTableContainer::WriteBaselines at engine.dll + 0x234C80
AUTOHOOK(CNetworkStringTableContainer__WriteBaselines, engine.dll + 0x234C80, __int64, __fastcall, (CNetworkStringTableContainer* thisptr, __int64 a2, __int64 a3))
{
    spdlog::info("=== WriteBaselines START ===");

    // Log how many tables we're processing using the proper API
    int tableCount = thisptr->GetNumTables();
    spdlog::info("  Table count: {}", tableCount);

    // Call original and log result
    __int64 result = CNetworkStringTableContainer__WriteBaselines(thisptr, a2, a3);

    spdlog::info("=== WriteBaselines END (result: {}) ===", result);
    return result;
}

// Hook individual table processing to see which one fails
// CNetworkStringTable::WriteUpdate - fixed signature with __int64 for last param
AUTOHOOK(CNetworkStringTable__WriteUpdate, engine.dll + 0x2352F0, __int64, __fastcall, (CNetworkStringTable* thisptr, __int64 startItem, void* buf, int clientIndex))
{
    const char* tableName = thisptr->GetTableName();
    int numStrings = thisptr->GetNumStrings();
    int maxStrings = thisptr->GetMaxStrings();

    // Check buffer state before write
    bf_write* buffer = reinterpret_cast<bf_write*>(buf);
    int bitsAvailableBefore = buffer->GetNumBitsLeft();
    int bitsWrittenBefore = buffer->GetNumBitsWritten();

    spdlog::info("  Writing table '{}': {} / {} strings (clientIndex: {}), buffer: {} / {} bits ({} bytes left)",
        tableName, numStrings, maxStrings, clientIndex,
        buffer->GetNumBitsWritten(), buffer->GetMaxNumBits(), buffer->GetNumBytesLeft());

    int result = CNetworkStringTable__WriteUpdate(thisptr, startItem, buf, clientIndex);

    // Log buffer state after write
    int bitsWritten = buffer->GetNumBitsWritten() - bitsWrittenBefore;
    spdlog::info("      After write: {} bits written, {} / {} total, overflow flag: {}",
        bitsWritten, buffer->GetNumBitsWritten(), buffer->GetMaxNumBits(), buffer->IsOverflowed());

    // Check if buffer overflowed during this write
    if (buffer->IsOverflowed())
    {
        spdlog::error("  !!! Table '{}' caused buffer OVERFLOW during write! Attempted to write {} bits, only {} available",
            tableName, bitsWritten, bitsAvailableBefore);
    }

    // Note: When clientIndex != -1, result will be different from numStrings (writing only 1 client's entry)
    // Only log mismatch when writing full baseline (clientIndex == -1)
    if (clientIndex == -1 && result != numStrings)
    {
        spdlog::error("  !!! Table '{}' BASELINE write mismatch! Expected {}, got {}", tableName, numStrings, result);
    }

    return result;
}


// NOTE:
// - engine.dll + 0x22A670 is the generic CNetMessage::WriteToBuffer wrapper (type + payload write),
//   not specifically SVC_CreateStringTable.
// - Use raw engine bitbuf field offsets derived from IDA to avoid depending on local bf_write ABI.
struct EngineBitWriteRaw
{
    void* m_pData;        // +0x00
    int m_nDataBytes;     // +0x08
    int m_nDataBits;      // +0x0C
    int m_iCurBit;        // +0x10
    bool m_bOverflow;     // +0x14
};

static std::mutex s_NetMsgWriteTraceMutex;
static std::unordered_set<void*> s_PoisonedWriteBuffers;
static ConVar* s_svCompressPlaylists = nullptr;
static bool(__fastcall* s_NET_BufferToBufferCompress)(void* pDest, __int64* pDestLen, const void* pSrc, int srcLen) = nullptr;
static thread_local std::vector<unsigned char> s_PlaylistsCompressedTls;
static bool s_LoggedMissingSvCompressPlaylists = false;

static bool ShouldCompressPlaylists()
{
    if (!s_svCompressPlaylists && g_pCVar)
        s_svCompressPlaylists = g_pCVar->FindVar("sv_compressPlaylists");

    if (!s_svCompressPlaylists)
    {
        if (!s_LoggedMissingSvCompressPlaylists)
        {
            spdlog::warn("sv_compressPlaylists not found yet; defaulting playlist compression to enabled");
            s_LoggedMissingSvCompressPlaylists = true;
        }
        return true;
    }

    return s_svCompressPlaylists->GetBool();
}

static inline int GetRawBitsWritten(void* buf)
{
    return reinterpret_cast<EngineBitWriteRaw*>(buf)->m_iCurBit;
}

static inline int GetRawMaxBits(void* buf)
{
    return reinterpret_cast<EngineBitWriteRaw*>(buf)->m_nDataBits;
}

static inline bool GetRawOverflow(void* buf)
{
    return reinterpret_cast<EngineBitWriteRaw*>(buf)->m_bOverflow;
}

static inline int GetRawBitsLeft(void* buf)
{
    int maxBits = GetRawMaxBits(buf);
    int curBits = GetRawBitsWritten(buf);
    return maxBits - curBits;
}

static inline int GetRawBytesLeft(void* buf)
{
    int bitsLeft = GetRawBitsLeft(buf);
    return bitsLeft > 0 ? (bitsLeft / 8) : 0;
}

static inline unsigned int GetMsgTypeFromVTable(void* msg)
{
    if (!msg)
        return 0xFFFFFFFFu;

    auto vtable = *reinterpret_cast<void***>(msg);
    if (!vtable || !vtable[8]) // +0x40
        return 0xFFFFFFFFu;

    using GetTypeFn = unsigned int(__fastcall*)(void*);
    return reinterpret_cast<GetTypeFn>(vtable[8])(msg);
}


// VSVC_Playlists::WriteToBuffer at engine.dll + 0x22BBD0
// Layout from IDA:
// +0x20: bool hasCompressedData
// +0x24: int dataBytes
// +0x28: void* dataPtr
// +0x30: int uncompressedBytes
AUTOHOOK(VSVC_Playlists__WriteToBuffer, engine.dll + 0x22BBD0, bool, __fastcall, (__int64 msg, __int64 buf))
{
    void* msgPtr = reinterpret_cast<void*>(msg);
    void* buffer = reinterpret_cast<void*>(buf);

    bool hasCompressedData = *reinterpret_cast<unsigned char*>(msg + 0x20) != 0;
    int dataBytes = *reinterpret_cast<int*>(msg + 0x24);
    void* dataPtr = *reinterpret_cast<void**>(msg + 0x28);
    int uncompressedBytes = *reinterpret_cast<int*>(msg + 0x30);

    const bool canTryCompress =
        !hasCompressedData && dataPtr && dataBytes > 0 && s_NET_BufferToBufferCompress
        && ShouldCompressPlaylists();

    bool usedTempCompression = false;
    unsigned char oldCompressedFlag = *reinterpret_cast<unsigned char*>(msg + 0x20);
    int oldDataBytes = dataBytes;
    void* oldDataPtr = dataPtr;
    int oldUncompressedBytes = uncompressedBytes;

    if (canTryCompress)
    {
        const size_t neededBytes = static_cast<size_t>(oldDataBytes) + 0x4000;
        if (s_PlaylistsCompressedTls.size() < neededBytes)
            s_PlaylistsCompressedTls.resize(neededBytes);

        __int64 compressedBytes = static_cast<__int64>(s_PlaylistsCompressedTls.size());
        if (s_NET_BufferToBufferCompress(s_PlaylistsCompressedTls.data(), &compressedBytes, oldDataPtr, oldDataBytes)
            && compressedBytes > 0 && compressedBytes < oldDataBytes)
        {
            *reinterpret_cast<unsigned char*>(msg + 0x20) = 1;
            *reinterpret_cast<int*>(msg + 0x24) = static_cast<int>(compressedBytes);
            *reinterpret_cast<void**>(msg + 0x28) = s_PlaylistsCompressedTls.data();
            *reinterpret_cast<int*>(msg + 0x30) = oldDataBytes;
            usedTempCompression = true;

            hasCompressedData = true;
            dataBytes = static_cast<int>(compressedBytes);
            dataPtr = s_PlaylistsCompressedTls.data();
            uncompressedBytes = oldDataBytes;
        }
    }

    int bitsBefore = GetRawBitsWritten(buffer);
    int bitsLeftBefore = GetRawBitsLeft(buffer);
    bool overflowBefore = GetRawOverflow(buffer);

    bool result = VSVC_Playlists__WriteToBuffer(msg, buf);

    if (usedTempCompression)
    {
        *reinterpret_cast<unsigned char*>(msg + 0x20) = oldCompressedFlag;
        *reinterpret_cast<int*>(msg + 0x24) = oldDataBytes;
        *reinterpret_cast<void**>(msg + 0x28) = oldDataPtr;
        *reinterpret_cast<int*>(msg + 0x30) = oldUncompressedBytes;
    }

    if (!result)
    {
        int bitsAfter = GetRawBitsWritten(buffer);
        bool overflowAfter = GetRawOverflow(buffer);

        spdlog::error("  !!! VSVC_Playlists::WriteToBuffer FAILED");
        spdlog::error(
            "      msg=0x{:X} vtbl=0x{:X} compressed={} dataBytes={} uncompressedBytes={} dataPtr=0x{:X}",
            reinterpret_cast<uintptr_t>(msgPtr),
            msgPtr ? reinterpret_cast<uintptr_t>(*reinterpret_cast<void**>(msgPtr)) : 0ull,
            hasCompressedData,
            dataBytes,
            uncompressedBytes,
            reinterpret_cast<uintptr_t>(dataPtr));
        spdlog::error(
            "      buffer: beforeBits={} bitsLeftBefore={} overflowBefore={}, afterBits={} overflowAfter={}",
            bitsBefore,
            bitsLeftBefore,
            overflowBefore,
            bitsAfter,
            overflowAfter);
    }
    else if (usedTempCompression)
    {
        spdlog::info(
            "  VSVC_Playlists compressed for send: {} -> {} bytes",
            oldDataBytes,
            dataBytes);
    }

    return result;
}


ON_DLL_LOAD_RELIESON(
	"engine.dll",
	NetworkStringTables,
	(ConCommand),
	[](CModule module)
	{
    s_NET_BufferToBufferCompress =
        module.Offset(0x218F50).RCast<decltype(s_NET_BufferToBufferCompress)>();

	AUTOHOOK_DISPATCH()
})
