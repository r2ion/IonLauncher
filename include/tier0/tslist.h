#ifndef TSLIST_H
#define TSLIST_H

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CAlignedMemAlloc
{
public:
	// Passed explicit parameters for 'this' pointer; the game expects one,
	// albeit unused. Do NOT optimize this away!
	typedef void* (*FnAlloc_t)(CAlignedMemAlloc* thisptr, size_t nSize, size_t nAlignment);
	typedef void (*FnFree_t)(CAlignedMemAlloc* thisptr, void* pMem);

	CAlignedMemAlloc(FnAlloc_t pAllocCallback, FnFree_t pFreeCallback);

	inline void* Alloc(size_t nSize, size_t nAlign = 0)
	{
		return m_pAllocCallback(this, nSize, nAlign);
	}
	inline void Free(void* pMem)
	{
		m_pFreeCallback(this, pMem);
	}

private:
	FnAlloc_t m_pAllocCallback;
	FnFree_t m_pFreeCallback;
};

extern CAlignedMemAlloc* g_pAlignedMemAlloc;

//-----------------------------------------------------------------------------
// Singleton aligned memalloc
//-----------------------------------------------------------------------------
inline CAlignedMemAlloc* AlignedMemAlloc()
{
	return g_pAlignedMemAlloc;
}

#endif // TSLIST_H
