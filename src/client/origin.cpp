#include "origin.h"
#include "core/convar/concommand.h"
#include "engine/client/clientstate.h"
#include "util/utils.h"

#include <algorithm>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>

DECLARE_MODULE(OriginHooks)

OriginRequestAuthCodeType OriginRequestAuthCode;
OriginGetPresenceType OriginGetPresence;
OriginQueryPresenceType OriginQueryPresence;
OriginQueryPresenceSyncType OriginQueryPresenceSync;
OriginGetErrorDescriptionType OriginGetErrorDescription;
OriginReadEnumerationSyncType OriginReadEnumerationSync;
OriginRequestFriendType OriginRequestFriend;
OriginSubscribePresenceType OriginSubscribePresence;
OriginQueryOffersType OriginQueryOffers;
OriginRequestFriendSyncType OriginRequestFriendSync;
OriginQueryUserIdSyncType OriginQueryUserIdSync;
OriginDestroyHandleType OriginDestroyHandle;
Tier0_GetOriginVersionStringType Tier0_GetOriginVersionString;

// 0xA0010000
std::unordered_map<__int64, std::string> g_IDPartySubMap;

const char* GetOriginErrorDescription(int errorCode)
{
    if (!OriginGetErrorDescription)
        return "unknown error";

    const char* description = OriginGetErrorDescription(errorCode);
    return description ? description : "unknown error";
}

void OnOriginAuthCodeReceived(void* context, const char** authCode, std::size_t authCodeLength, int errorCode)
{
    std::unique_ptr<std::shared_ptr<OriginTokenRequestState_t>> callbackState(static_cast<std::shared_ptr<OriginTokenRequestState_t>*>(context));
    if (!callbackState || !*callbackState)
        return;

    const std::shared_ptr<OriginTokenRequestState_t> state = *callbackState;
    {
        std::scoped_lock lock(state->mutex);
        state->errorCode = errorCode;
        if (errorCode == 0 && authCode && *authCode && authCodeLength > 0)
            state->token.assign(*authCode, authCodeLength);
        state->isComplete = true;
    }
    state->completion.notify_one();
}

// do not use this on the main thread, this is blocking
std::optional<std::string> GetNewOriginToken(std::chrono::milliseconds timeout)
{
    if (timeout <= std::chrono::milliseconds::zero())
    {
        spdlog::error("Failed to get origin token: timeout must be positive");
        return std::nullopt;
    }

    int64_t userId = 0;
    if (g_pLocalPlayerUserID)
        userId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);
    if (userId <= 0)
    {
        spdlog::error("Failed to get origin token: invalid local player user ID");
        return std::nullopt;
    }

    const std::chrono::milliseconds requestTimeout = std::min(timeout, std::chrono::milliseconds(std::numeric_limits<int>::max()));
    const std::shared_ptr<OriginTokenRequestState_t> state = std::make_shared<OriginTokenRequestState_t>();
    auto* callbackState = new std::shared_ptr<OriginTokenRequestState_t>(state);
    const int requestResult = OriginRequestAuthCode(userId, "TITANFALL2-PC-SERVER", OnOriginAuthCodeReceived, callbackState,
                                                    static_cast<int>(requestTimeout.count()), nullptr);
    if (requestResult != 0)
    {
        delete callbackState;
        spdlog::error("Failed to request origin token: {} ({:#010x})", GetOriginErrorDescription(requestResult),
                      static_cast<uint32_t>(requestResult));
        return std::nullopt;
    }

    std::unique_lock lock(state->mutex);
    if (!state->completion.wait_for(lock, requestTimeout, [&state] { return state->isComplete; }))
    {
        spdlog::error("Failed to get origin token: timeout after {} ms", requestTimeout.count());
        return std::nullopt;
    }

    if (state->errorCode != 0)
    {
        spdlog::error("Failed to get origin token: {} ({:#010x})", GetOriginErrorDescription(state->errorCode),
                      static_cast<uint32_t>(state->errorCode));
        return std::nullopt;
    }
    if (state->token.empty())
    {
        spdlog::error("Failed to get origin token: Origin returned an empty token");
        return std::nullopt;
    }

    return std::move(state->token);
}

DECLARE_HOOK(OriginSub185AE0, engine.dll + 0x185AE0, [](auto& hook, __int64 uid, unsigned int a2, FriendPresence* pFriendPresence) -> __int64
{
    if (pFriendPresence->GamePresence)
    {
        auto match_sub = *(const char**)&pFriendPresence->GamePresence[0x20];

        if (!IsBadReadPtr2((void*)match_sub))
        {
            if (match_sub && match_sub[0] != '\0' && strncmp(match_sub, "p_PC_", 5) == 0)
            {
                g_IDPartySubMap.insert(std::make_pair(pFriendPresence->uid, std::string(match_sub)));
                spdlog::info("GamePresence for uid {}: {}", uid, match_sub);
            }
            else
            {
                if (g_IDPartySubMap.contains(pFriendPresence->uid))
                    g_IDPartySubMap.erase(pFriendPresence->uid);
            }
        }
        else
        {
            if (g_IDPartySubMap.contains(pFriendPresence->uid))
                g_IDPartySubMap.erase(pFriendPresence->uid);
        }
    }

    return hook.Original(uid, a2, pFriendPresence);
})

DECLARE_HOOK(UpdateFriendsList, engine.dll + 0x184000, [](auto& hook, __int64 a1, __int64 a2, unsigned __int64 a3, __int64 a4, int a5) -> __int64
{
    spdlog::info("UpdateFriendsListHook called with a1: {}, a2: {}, a3: {}, a4: {}, a5: {}", a1, a2, a3, a4, a5);
    return hook.Original(a1, a2, a3, a4, a5);
})

void PresenceCallback(__int64 a1, __int64 a2, __int64 a3, __int64 a4)
{
    spdlog::log(spdlog::level::info, "PresenceCallback called with a1: {}, a2: {}, a3: {}, a4: {}", a1, a2, a3, a4);
}

void SyncPresenceCallback(__int64 a1, __int64 a2, __int64 a3, __int64 a4, unsigned __int64 a5, __int64 a6)
{

    //  v11 = sub_17E420(saturated_mul(amount, 0x60uLL));
    // if (!(unsigned int)OriginReadEnumerationSync(a2, v11, 96 * (int)amount, 0LL, 0xFFFFFFFFLL, (__int64)&a6))
    spdlog::info("SyncPresenceCallback called with a1: {}, a2: {}, a3: {}, a4: {}, a5: {}, a6: {}", a1, a2, a3, a4, a5, a6);

    void* buffer1 = new char[96 * a3]; // Buffer for the presence data
    __int64 outvalue = 0;
    auto ret = OriginReadEnumerationSync(a2, buffer1, 96 * a3, 0LL, 0xFFFFFFFFLL, &outvalue);
    if (ret != 0)
    {
        auto reason = OriginGetErrorDescription(ret);
        spdlog::error("Failed to read enum for presence. Error code: {}. Reason: {}", ret, reason);
        delete[] buffer1;
        return;
    }
    spdlog::info("Read enum for presence successfully. Buffer1: {}", buffer1);
}

void ConCommand_ns_fetch_presence(const CCommand& args)
{
    auto localUserId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);
    const char* userIdStr = args.Arg(1);
    if (!userIdStr || userIdStr[0] == '\0')
    {
        spdlog::error("No user ID provided for presence fetch.");
        return;
    }

    __int64 userId = _strtoi64(userIdStr, nullptr, 10);
    std::array<const char*, 10> userIds; // Assuming we want to subscribe to presence for 5 user IDs
    for (int i = 0; i < 1; ++i)          // Assuming we want to subscribe to presence for 5 user IDs
        userIds[i] = userIdStr;          // to subscribe to presence

    void* buffer1 = new char[0x1000]; // Allocate a buffer for the presence data
    int64_t outHandle = 0;            // Initialize outHandle to 0
    void* buffer3 = new char[0x1000]; // Allocate a third buffer for the presence data
    void* buffer4 = new char[0x1000]; // Allocate a third buffer for the presence data

    int64_t enum2 = 0; // Initialize enum2 to 0
    auto ret = OriginQueryPresence(localUserId, userIds.data(), 1, (int64_t)SyncPresenceCallback, buffer1, 1, buffer3);
    if (ret != 0)
    {
        auto reason = OriginGetErrorDescription(ret);
        spdlog::error("Failed to subscribe to presence for user ID: {}. Error code: {}. Reason: {}", userId, ret, reason);
        return;
    }
}

void ConCommand_ns_send_friend_request(const CCommand& args)
{
    auto localUserId = _strtoi64(g_pLocalPlayerUserID, nullptr, 10);
    const char* userIdStr = args.Arg(1);
    if (!userIdStr || userIdStr[0] == '\0')
    {
        spdlog::error("No user ID provided for friend request.");
        return;
    }

    __int64 userId = _strtoi64(userIdStr, nullptr, 10);

    auto ret = OriginRequestFriendSync(localUserId, userId, 300); // Request friend sync for the user ID
    auto reason = OriginGetErrorDescription(ret);
    if (ret != 0)
    {
        spdlog::error("Failed to send friend request for user ID: {}. Error code: {}. Reason: {}", userId, ret, reason);
        return;
    }
    else
    {
        spdlog::info(" Sent friend request for user ID: {}. Error code: {}. Reason: {}", userId, ret, reason);
    }
}

void ConCommand_ns_print_origin_code(const CCommand& args)
{
    const char* codeStr = args.Arg(1);
    if (!codeStr || codeStr[0] == '\0')
    {
        spdlog::error("No code provided to print.");
        return;
    }
    uint64_t code = std::stoull(codeStr);
    auto reason = OriginGetErrorDescription(code);
    spdlog::info("Origin error code: {}. Reason: {}", code, reason);
}

void ConCommand_ns_query_userid(const CCommand& args)
{
    if (args.ArgC() < 2)
        return;

    const char* userName = args.Arg(1);
    void* sdkContext = Tier0_GetOriginVersionString();
    void* outRequest = nullptr;

    auto ret = OriginQueryUserIdSync(userName, sdkContext, 2000, &outRequest);

    if (ret != 0)
    {
        OriginDestroyHandle(outRequest);
        const char* reason = OriginGetErrorDescription(ret);
        spdlog::error("OriginQueryUserIdSync failed with error code {}: {}", ret, reason);
        return;
    }

    uintptr_t outRequestAddr = reinterpret_cast<uintptr_t>(outRequest);
    auto pData = *reinterpret_cast<uint8_t**>(outRequestAddr + 0xB8);
    uint64_t userId = *reinterpret_cast<uint64_t*>(pData + 0x40);
    OriginDestroyHandle(outRequest);

    spdlog::info("UserID: {}", userId);
}

DECLARE_HOOK(CheckIfOriginIsInstalled, OriginSDK.dll + 0xa1850, [](auto& hook, void* a1) -> bool
{
    if (!strstr(GetCommandLineA(), "-noOriginStartup"))
        return hook.Original(a1);

    if (hook.Original(a1))
    {
        NS::log::NORTHSTAR->warn("Origin is NOT installed according to OriginSDK's check (HKLM\\SOFTWARE\\Wow6432Node\\Origin\\ClientPath is "
                                 "missing/empty).");
        NS::log::NORTHSTAR->warn(
            "We are bypassing this check in case LSX server is remotely ran (such as in Linux in another WINE prefix), but note that "
            "things will fail if Origin is actually not running.");
    }

    return true;
});

DECLARE_HOOK(TryToStartOrigin, OriginSDK.dll + 0xa19b0, [](auto& hook, void* a1) -> uint64_t
{
    if (!strstr(GetCommandLineA(), "-noOriginStartup"))
        return hook.Original(a1);

    if (hook.Original(a1) != 0)
    {
        NS::log::NORTHSTAR->warn(
            "Origin process has failed to start. We are ignoring this and let it fail on LSX connection attempt, because LSX might "
            "still be up regardless, even if this failed.");
    }
    return 0;
});

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientOrigin, ConCommand, [](CModule module)
{
    DISPATCH_MODULE(OriginHooks)
    RegisterConCommand("ns_fetchpres", ConCommand_ns_fetch_presence, "Fetch presence for uid", FCVAR_CLIENTDLL);
    RegisterConCommand("ns_send_friend_request", ConCommand_ns_send_friend_request, "Send friend request to uid", FCVAR_CLIENTDLL);
    RegisterConCommand("ns_query_userid", ConCommand_ns_query_userid, "Query user ID from username", FCVAR_CLIENTDLL);
    RegisterConCommand("ns_print_origin_code", ConCommand_ns_print_origin_code, "Print origin error code description for code", FCVAR_CLIENTDLL);
    DISPATCH_MODULE(OriginHooks)
    g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration("string", "GetUID", "entity player", "Get uid of a player",
                                                            [](HSQUIRRELVM sqvm) -> SQRESULT
    {
        auto pPlayer = g_pSquirrel[ScriptContext::CLIENT]->getentity<CBaseEntity>(sqvm, 1);
        if (!pPlayer)
        {
            spdlog::error("print_lobby_uids: Invalid player entity.");
            return SQRESULT_ERROR;
        }
        auto uid = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pPlayer) + 0x2020);
        char uidBuffer[64];
        sprintf_s(uidBuffer, "%llu", uid);
        g_pSquirrel[ScriptContext::CLIENT]->pushstring(sqvm, uidBuffer, -1);

        return SQRESULT_NOTNULL;
    });
})

// static int OriginReadEnumerationSyncHook(__int64 a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5, __int64 a6) {
// 	// print all the parameters in hex format

// 	spdlog::info("OriginReadEnumerationSyncHook called with a1: {}, a2: {}, a3: {}, a4: {}, a5: {}, a6: {}", a1, a2, a3, a4, a5,a6);
// 	return OriginReadEnumerationSync(a1, a2, a3, a4, a5,a6);
// }

ON_DLL_LOAD("OriginSDK.dll", OriginSDK, [](CModule module)
{
    // takes 5 params: user_id (g_pLocalPlayerUserID), the game ("TITANFALL2-PC-SERVER"), callback func to strcpy the token to which looks
    // like void(*)(__int64 a1, char* a2), and 3 ints which idk what they are but are 0, 30000 and 0 by default. probably some token ttl
    // stuff
    OriginRequestAuthCode = module.GetExportedFunction("OriginRequestAuthCode").RCast<OriginRequestAuthCodeType>();
    OriginGetPresence = module.GetExportedFunction("OriginGetPresence").RCast<OriginGetPresenceType>();
    OriginQueryPresence = module.GetExportedFunction("OriginQueryPresence").RCast<OriginQueryPresenceType>();
    OriginGetErrorDescription = module.GetExportedFunction("OriginGetErrorDescription").RCast<OriginGetErrorDescriptionType>();
    OriginReadEnumerationSync = module.GetExportedFunction("OriginReadEnumerationSync").RCast<OriginReadEnumerationSyncType>();
    OriginRequestFriend = module.GetExportedFunction("OriginRequestFriend").RCast<OriginRequestFriendType>();
    OriginSubscribePresence = module.GetExportedFunction("OriginSubscribePresence").RCast<OriginSubscribePresenceType>();
    OriginQueryPresenceSync = module.GetExportedFunction("OriginQueryPresenceSync").RCast<OriginQueryPresenceSyncType>();
    OriginQueryOffers = module.GetExportedFunction("OriginQueryOffers").RCast<OriginQueryOffersType>();
    OriginRequestFriendSync = module.GetExportedFunction("OriginRequestFriendSync").RCast<OriginRequestFriendSyncType>();
    OriginQueryUserIdSync = module.GetExportedFunction("OriginQueryUserIdSync").RCast<OriginQueryUserIdSyncType>();
    OriginDestroyHandle = module.GetExportedFunction("OriginDestroyHandle").RCast<OriginDestroyHandleType>();
    DISPATCH_MODULE(OriginHooks)
})

ON_DLL_LOAD("tier0.dll", Tier0, [](CModule module)
{ Tier0_GetOriginVersionString = module.GetExportedFunction("Tier0_GetOriginVersionString").RCast<Tier0_GetOriginVersionStringType>(); })
