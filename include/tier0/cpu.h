#pragma once

#include <cstddef>
#include <cstdint>

struct CpuIdResult_t
{
	unsigned long eax;
	unsigned long ebx;
	unsigned long ecx;
	unsigned long edx;

	void Reset()
	{
		eax = ebx = ecx = edx = 0;
	}
};

union CpuBrand_t
{
	CpuIdResult_t cpuid[3];
	char name[49];
};

struct CPUInformation
{
	int m_Size;

	bool m_bRDTSC : 1;
	bool m_bCMOV : 1;
	bool m_bFCMOV : 1;
	bool m_bSSE : 1;
	bool m_bSSE2 : 1;
	bool m_b3DNow : 1;
	bool m_bMMX : 1;
	bool m_bHT : 1;

	std::uint8_t m_nLogicalProcessors;
	std::uint8_t m_nPhysicalProcessors;
	std::int64_t m_Speed;
	char* m_szProcessorID;
	char* m_szProcessorBrand;

	bool m_bSSE3 : 1;
	bool m_bSSSE3 : 1;
	bool m_bSSE4a : 1;
	bool m_bSSE41 : 1;
	bool m_bSSE42 : 1;
	bool m_bPOPCNT : 1;
	bool m_bAVX : 1;
	bool m_bHRVSR : 1;

	std::uint32_t m_nModel;
	std::uint32_t m_nFeatures[3];
	std::uint32_t m_nL1CacheSizeKb;
	std::uint32_t m_nL1CacheDesc;
	std::uint32_t m_nL2CacheSizeKb;
	std::uint32_t m_nL2CacheDesc;
	std::uint32_t m_nL3CacheSizeKb;
	std::uint32_t m_nL3CacheDesc;

	CPUInformation()
		: m_Size(0),
		  m_bRDTSC(false),
		  m_bCMOV(false),
		  m_bFCMOV(false),
		  m_bSSE(false),
		  m_bSSE2(false),
		  m_b3DNow(false),
		  m_bMMX(false),
		  m_bHT(false),
		  m_nLogicalProcessors(0),
		  m_nPhysicalProcessors(0),
		  m_Speed(0),
		  m_szProcessorID(nullptr),
		  m_szProcessorBrand(nullptr),
		  m_bSSE3(false),
		  m_bSSSE3(false),
		  m_bSSE4a(false),
		  m_bSSE41(false),
		  m_bSSE42(false),
		  m_bPOPCNT(false),
		  m_bAVX(false),
		  m_bHRVSR(false),
		  m_nModel(0),
		  m_nFeatures {},
		  m_nL1CacheSizeKb(0),
		  m_nL1CacheDesc(0),
		  m_nL2CacheSizeKb(0),
		  m_nL2CacheDesc(0),
		  m_nL3CacheSizeKb(0),
		  m_nL3CacheDesc(0)
	{
	}
};

static_assert(sizeof(CpuIdResult_t) == 0x10);
static_assert(sizeof(CpuBrand_t) == 0x34);
static_assert(alignof(CPUInformation) == 0x8);
static_assert(sizeof(CPUInformation) == 0x50);
static_assert(offsetof(CPUInformation, m_Size) == 0x0);
static_assert(offsetof(CPUInformation, m_nLogicalProcessors) == 0x5);
static_assert(offsetof(CPUInformation, m_nPhysicalProcessors) == 0x6);
static_assert(offsetof(CPUInformation, m_Speed) == 0x8);
static_assert(offsetof(CPUInformation, m_szProcessorID) == 0x10);
static_assert(offsetof(CPUInformation, m_szProcessorBrand) == 0x18);
static_assert(offsetof(CPUInformation, m_nModel) == 0x24);
static_assert(offsetof(CPUInformation, m_nFeatures) == 0x28);
static_assert(offsetof(CPUInformation, m_nL1CacheSizeKb) == 0x34);
static_assert(offsetof(CPUInformation, m_nL3CacheDesc) == 0x48);

bool CheckSSE3Technology();
bool CheckSSSE3Technology();
bool CheckSSE41Technology();
bool CheckSSE42Technology();
bool CheckSSE4aTechnology();

const char* GetProcessorVendorId();
const char* GetProcessorBrand(bool removePadding);
const CPUInformation& GetCPUInformation();

void CheckSystemCPUForSSE2();
void CheckSystemCPUForSSE3();
void CheckSystemCPUForSupplementalSSE3();
void CheckSystemCPUForPopCount();
void CheckSystemCPU();
