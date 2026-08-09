#pragma once

#include "tier0/memalloc.h"

class CCommandLine
{
public:
	// based on the defs in the 2013 source sdk, but for some reason has an extra function (may be another CreateCmdLine overload?)
	// these seem to line up with what they should be though
	virtual void CreateCmdLine(const char* commandline) = 0;
	virtual void CreateCmdLine(int argc, char** argv) = 0;
	virtual void unknown() = 0;
	virtual const char* GetCmdLine(void) const = 0;

	virtual const char* CheckParm(const char* psz, const char** ppszValue = 0) const = 0;
	virtual void RemoveParm() const = 0;
	virtual void AppendParm(const char* pszParm, const char* pszValues) = 0;

	virtual const char* ParmValue(const char* psz, const char* pDefaultVal = 0) const = 0;
	virtual int ParmValue(const char* psz, int nDefaultVal) const = 0;
	virtual float ParmValue(const char* psz, float flDefaultVal) const = 0;

	virtual int ParmCount() const = 0;
	virtual int FindParm(const char* psz) const = 0;
	virtual const char* GetParm(int nIndex) const = 0;
	virtual void SetParm(int nIndex, char const* pParm) = 0;

	// virtual const char** GetParms() const {}
};

extern IMemAlloc* g_pMemAllocSingleton;

typedef CCommandLine* (*CommandLineType)();
extern CommandLineType CommandLine;

typedef double (*Plat_FloatTimeType)();
extern Plat_FloatTimeType g_PlatFloatTime;

bool ThreadInMainThread();
bool ThreadInServerFrameThread();

void TryCreateGlobalMemAlloc();
