#ifndef TIER0_MEMALLOC_H
#define TIER0_MEMALLOC_H

#include <cstddef>

class IMemAlloc
{
public:
	virtual void* InternalAlloc(std::size_t nSize, const char* pFileName, int nLine) = 0; // 0
	virtual void* Alloc(std::size_t nSize) = 0; // 1
	virtual void* InternalRealloc(
		void* pMem, std::size_t nSize, const char* pFileName, int nLine) = 0; // 2
	virtual void* Realloc(void* pMem, std::size_t nSize) = 0; // 3
	virtual void InternalFree(void* pMem, const char* pFileName, int nLine) = 0; // 4
	virtual void Free(void* pMem) = 0; // 5
	virtual void Unknown06() = 0; // 6
	virtual void Unknown07() = 0; // 7
	virtual std::size_t GetSize(void* pMem) = 0; // 8
	virtual void Unknown09() = 0; // 9
	virtual void Unknown10() = 0; // 10
	virtual void Unknown11() = 0; // 11
	virtual void Unknown12() = 0; // 12
	virtual void Unknown13() = 0; // 13
	virtual void Unknown14() = 0; // 14
	virtual void Unknown15() = 0; // 15
	virtual void Unknown16() = 0; // 16
	virtual void Unknown17() = 0; // 17
	virtual void DumpStats() = 0; // 18
	virtual void DumpStatsFileBase(const char* pchFileBase) = 0; // 19
	virtual void Unknown20() = 0; // 20
	virtual void Unknown21() = 0; // 21
	virtual void Unknown22() = 0; // 22
	virtual void Unknown23() = 0; // 23
	virtual void Unknown24() = 0; // 24
	virtual void Unknown25() = 0; // 25
};

static_assert(sizeof(IMemAlloc) == sizeof(void*));

#if defined(MSVC) && ( defined(_DEBUG) || defined(USE_MEM_DEBUG) )

#pragma warning(disable:4290)
#pragma warning(push)
//#include <typeinfo.h>

// MEM_DEBUG_CLASSNAME is opt-in.
// Note: typeid().name() is not threadsafe, so if the project needs to access it in multiple threads
// simultaneously, it'll need a mutex.

#define MEM_ALLOC_CREDIT_(tag)	((void)0) // Stubbed for now.

#if defined(_CPPRTTI) && defined(MEM_DEBUG_CLASSNAME)

template <typename T> const char* MemAllocClassName(T* p)
{
	static const char* pszName = typeid(*p).name(); // @TODO: support having debug heap ignore certain allocations, and ignore memory allocated here [5/7/2009 tom]
	return pszName;
}

#define MEM_ALLOC_CREDIT_CLASS()	MEM_ALLOC_CREDIT_( MemAllocClassName( this ) )
#define MEM_ALLOC_CLASSNAME(type) (typeid((type*)(0)).name())
#else
#define MEM_ALLOC_CREDIT_CLASS()	MEM_ALLOC_CREDIT_( __FILE__ )
#define MEM_ALLOC_CLASSNAME(type) (__FILE__)
#endif

// MEM_ALLOC_CREDIT_FUNCTION is used when no this pointer is available ( inside 'new' overloads, for example )
#ifdef _MSC_VER
#define MEM_ALLOC_CREDIT_FUNCTION()		MEM_ALLOC_CREDIT_( __FUNCTION__ )
#else
#define MEM_ALLOC_CREDIT_FUNCTION() (__FILE__)
#endif

#pragma warning(pop)
#else
#define MEM_ALLOC_CREDIT_CLASS()
#define MEM_ALLOC_CLASSNAME(type) NULL
#define MEM_ALLOC_CREDIT_FUNCTION()
#endif

#define MEM_ALLOC_CREDIT() // Stubbed

#endif /* TIER0_MEMALLOC_H */
