#pragma once

#define PAK_MAX_LOADED_PAKS 512
#define PAK_MAX_LOADED_PAKS_MASK (PAK_MAX_LOADED_PAKS-1)

typedef int PakHandle_t;
typedef uint64_t PakGuid_t;

struct PakAssetShort_s
{
	PakGuid_t m_Guid;
	uint32_t m_nTrackerIndex;
	uint32_t unk;
};

struct PakAssetBinding_s
{
	char type[4];
	char* description;
	void* loadAssetFunc;
	void* unloadAssetFunc;
	void* replaceAssetFunc;
	void* allocator;

	uint32_t N00009080;
	uint32_t N0000908D;
	uint32_t N00009081;
	uint32_t N0000908F;
	uint32_t N00009082;
	uint32_t N00009098;
	uint32_t N00009090;
	uint32_t N0000909A;

	void* allocator_duplicate;
	void* page;
};

struct PakGlobalState_s
{

};

extern PakGlobalState_s* g_pakGlobalState;
