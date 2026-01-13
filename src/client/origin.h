#pragma once

typedef void (*OriginAuthcodeStrcpyCallbackType)(__int64 a1, __int64* a2);
typedef __int64 (*OriginRequestAuthCodeType)(
	__int64 userId, const char* game, OriginAuthcodeStrcpyCallbackType callback, __int64* a4, int a5, __int64* a6);
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

typedef int (*OriginQueryOffersType)(
	__int64 userId, const char** offerId, int numOffers, __int64 a4, int64_t a5, __int64 a6, __int64 a7, int a8, __int64 a9);

typedef int (*OriginRequestFriendSyncType)(__int64 userId, __int64 friendId, __int64 timeout);

extern OriginGetPresenceType OriginGetPresence;

typedef const char* (*OriginGetErrorDescriptionType)(int errorCode);
extern OriginGetErrorDescriptionType OriginGetErrorDescription;

//__int64 OriginReadEnumerationSync(int a1, int a2, int a3, __int64 a4, __int64 a5, __int64 a6)
typedef int (*OriginReadEnumerationSyncType)(int64_t a1, void* a2, int64_t a3, __int64 lower, __int64 upper, __int64* a6);
extern OriginReadEnumerationSyncType OriginReadEnumerationSync;

typedef int (*OriginRequestFriendType)(int a1, int a2, int a3, __int64 a4, __int64 a5);
extern OriginRequestFriendType OriginRequestFriend;

std::string* GetNewOriginToken(int timeoutSeconds);

extern std::unordered_map<__int64, std::string> g_IDPartySubMap;
