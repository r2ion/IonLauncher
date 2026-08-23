#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

struct OriginTokenRequestState_t
{
    std::mutex mutex;
    std::condition_variable completion;
    std::string token;
    int errorCode = 0;
    bool isComplete = false;
};

class FriendPresence
{
  public:
    int64_t uid;        // 0x0000
    int32_t state;      // 0x0008
    char pad_000C[4];   // 0x000C
    char* Title_ID;     // 0x0010
    char* MP_ID;        // 0x0018
    char* Title;        // 0x0020
    char* Presence;     // 0x0028
    char* GamePresence; // 0x0030
    char* empty;        // 0x0038
    char* empty2;       // 0x0040
};

using OriginAuthCodeCallbackType = void (*)(void* context, const char** authCode, std::size_t authCodeLength, int errorCode);
using OriginRequestAuthCodeType = int (*)(int64_t userId, const char* serviceName, OriginAuthCodeCallbackType callback, void* context,
                                          int timeoutMilliseconds, const char* authCodeType);
extern OriginRequestAuthCodeType OriginRequestAuthCode;
enum OriginPresenceEnum
{
    UNK,
    IS_OFFLINE,
    IS_ONLINE,
    IN_GAME,
    BUSY,
    AWAY,
    IS_IN_PARTY,
    IS_IN_GAME_PARTY,
    IS_INVITE_ONLY
};
typedef int (*OriginGetPresenceType)(__int64 userId, void* presenceData, int a3, __int64 a4, void* a5, __int64 a6, int a7, __int64* a8);

typedef int (*OriginQueryPresenceType)(__int64 userId, void* userIds, int numIds, __int64 a4, void* a5, __int64 a6, void* a7);

typedef int (*OriginQueryPresenceSyncType)(__int64 userId, void* userIds, int numIds, void* a4, int a5, __int64* a6);

typedef int (*OriginSubscribePresenceType)(__int64 userId, void* a2, int64_t a3);
extern OriginSubscribePresenceType OriginSubscribePresence;

typedef int (*OriginQueryOffersType)(__int64 userId, const char** offerId, int numOffers, __int64 a4, int64_t a5, __int64 a6, __int64 a7, int a8,
                                     __int64 a9);

typedef int (*OriginRequestFriendSyncType)(__int64 userId, __int64 friendId, __int64 timeout);

extern OriginGetPresenceType OriginGetPresence;

typedef const char* (*OriginGetErrorDescriptionType)(int errorCode);
extern OriginGetErrorDescriptionType OriginGetErrorDescription;

//__int64 OriginReadEnumerationSync(int a1, int a2, int a3, __int64 a4, __int64 a5, __int64 a6)
typedef int (*OriginReadEnumerationSyncType)(int64_t a1, void* a2, int64_t a3, __int64 lower, __int64 upper, __int64* a6);
extern OriginReadEnumerationSyncType OriginReadEnumerationSync;

typedef int (*OriginRequestFriendType)(int a1, int a2, int a3, __int64 a4, __int64 a5);
extern OriginRequestFriendType OriginRequestFriend;

std::optional<std::string> GetNewOriginToken(std::chrono::milliseconds timeout);

extern std::unordered_map<__int64, std::string> g_IDPartySubMap;

typedef int (*OriginQueryUserIdSyncType)(const char* userName, const void* originContext, __int64 timeoutMs, void* outRequest);
typedef int (*OriginDestroyHandleType)(void* handle);
typedef void* (*Tier0_GetOriginVersionStringType)();
