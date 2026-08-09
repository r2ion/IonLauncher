#include "tier1/convar.h"
#include "core/tier0.h"
#include "tier1/bitbuf.h"
#include "logging/logging.h"
#include "tier0/module.h"
#include "tier1/cvar.h"
#include <vector>

DECLARE_MODULE(NetworkStringTablesHooks)

static bool(__fastcall* s_NET_BufferToBufferCompress)(void* pDest, __int64* pDestLen, const void* pSrc, int srcLen) = nullptr;

struct VSVC_PlaylistsLayout
{
    char pad0[0x20];
    bool hasCompressedData; // +0x20
    char pad1[0x3];         // +0x21
    int dataBytes;          // +0x24
    void* dataPtr;          // +0x28
    int uncompressedBytes;  // +0x30
};

static_assert(offsetof(VSVC_PlaylistsLayout, hasCompressedData) == 0x20);
static_assert(offsetof(VSVC_PlaylistsLayout, dataBytes) == 0x24);
static_assert(offsetof(VSVC_PlaylistsLayout, dataPtr) == 0x28);
static_assert(offsetof(VSVC_PlaylistsLayout, uncompressedBytes) == 0x30);

DECLARE_HOOK(SVC_Playlists::WriteToBuffer, engine.dll + 0x22BBD0, [](auto& hook, __int64 msg, bf_write* buffer) -> bool
{
    static thread_local std::vector<unsigned char> s_PlaylistsCompressedTls;

    void* msgPtr = reinterpret_cast<void*>(msg);
    auto* msgLayout = reinterpret_cast<VSVC_PlaylistsLayout*>(msgPtr);
    bool hasCompressedData = msgLayout->hasCompressedData;
    int dataBytes = msgLayout->dataBytes;
    void* dataPtr = msgLayout->dataPtr;
    int uncompressedBytes = msgLayout->uncompressedBytes;
	static ConVar* Cvar_sv_compress_playlists = g_pCVar->FindVar("sv_compressPlaylists");
    const bool canTryCompress =
        !hasCompressedData && dataPtr && dataBytes > 0 && s_NET_BufferToBufferCompress && Cvar_sv_compress_playlists->GetBool();

    bool usedTempCompression = false;
    unsigned char oldCompressedFlag = msgLayout->hasCompressedData ? 1 : 0;
    int oldDataBytes = dataBytes;
    void* oldDataPtr = dataPtr;
    int oldUncompressedBytes = uncompressedBytes;

    if (canTryCompress)
    {
        const size_t neededBytes = static_cast<size_t>(oldDataBytes) + 0x4000;
        if (s_PlaylistsCompressedTls.size() < neededBytes)
            s_PlaylistsCompressedTls.resize(neededBytes);

        __int64 compressedBytes = static_cast<__int64>(s_PlaylistsCompressedTls.size());
        if (s_NET_BufferToBufferCompress(s_PlaylistsCompressedTls.data(), &compressedBytes, oldDataPtr, oldDataBytes) && compressedBytes > 0 &&
            compressedBytes < oldDataBytes)
        {
            msgLayout->hasCompressedData = true;
            msgLayout->dataBytes = static_cast<int>(compressedBytes);
            msgLayout->dataPtr = s_PlaylistsCompressedTls.data();
            msgLayout->uncompressedBytes = oldDataBytes;
            usedTempCompression = true;

            hasCompressedData = true;
            dataBytes = static_cast<int>(compressedBytes);
            dataPtr = s_PlaylistsCompressedTls.data();
            uncompressedBytes = oldDataBytes;
        }
    }

    int bitsBefore = buffer->GetNumBitsWritten();
    int bitsLeftBefore = buffer->GetNumBitsLeft();
    bool overflowBefore = buffer->IsOverflowed();

    bool result = hook.Original(msg, buffer);

    if (usedTempCompression)
    {
        msgLayout->hasCompressedData = (oldCompressedFlag != 0);
        msgLayout->dataBytes = oldDataBytes;
        msgLayout->dataPtr = oldDataPtr;
        msgLayout->uncompressedBytes = oldUncompressedBytes;
    }

    if (!result)
    {
        int bitsAfter = buffer->GetNumBitsWritten();
        bool overflowAfter = buffer->IsOverflowed();

        spdlog::error("  !!! VSVC_Playlists::WriteToBuffer FAILED");
        spdlog::error("      msg=0x{:X} vtbl=0x{:X} compressed={} dataBytes={} uncompressedBytes={} dataPtr=0x{:X}",
                      reinterpret_cast<uintptr_t>(msgPtr), msgPtr ? reinterpret_cast<uintptr_t>(*reinterpret_cast<void**>(msgPtr)) : 0ull,
                      hasCompressedData, dataBytes, uncompressedBytes, reinterpret_cast<uintptr_t>(dataPtr));
        spdlog::error("      buffer: beforeBits={} bitsLeftBefore={} overflowBefore={}, afterBits={} overflowAfter={}", bitsBefore, bitsLeftBefore,
                      overflowBefore, bitsAfter, overflowAfter);
    }
    else if (usedTempCompression)
    {
        spdlog::info("  SVC_Playlists compressed for send: {} -> {} bytes", oldDataBytes, dataBytes);
    }

    return result;
})

ON_DLL_LOAD_RELIESON("engine.dll", NetworkStringTables, (ConCommand), [](CModule module)
{
    s_NET_BufferToBufferCompress = module.Offset(0x218F50).RCast<decltype(s_NET_BufferToBufferCompress)>();

	DISPATCH_MODULE(NetworkStringTablesHooks)
})
