#include "core/tier0.h"
#include "tier0/module.h"
#include "logging/logging.h"
#include "core/convar/convar.h"
#include <vector>
AUTOHOOK_INIT()

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
