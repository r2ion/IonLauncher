#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>

#if defined(_MSC_VER)
#define REFCOUNT_NOVTABLE __declspec(novtable)
#else
#define REFCOUNT_NOVTABLE
#endif

class IRefCounted
{
  public:
    virtual int AddRef() = 0;
    virtual int Release() = 0;
};

template <class REFCOUNTED_ITEM_PTR>
inline int SafeRelease(REFCOUNTED_ITEM_PTR& pRef)
{
    REFCOUNTED_ITEM_PTR* ppRef = &pRef;
    if (*ppRef)
    {
        const int result = (*ppRef)->Release();
        *ppRef = nullptr;
        return result;
    }
    return 0;
}

template <class T = IRefCounted>
class CAutoRef
{
  public:
    explicit CAutoRef(T* pRef) : m_pRef(pRef)
    {
        if (m_pRef)
            m_pRef->AddRef();
    }

    ~CAutoRef()
    {
        if (m_pRef)
            m_pRef->Release();
    }

  private:
    T* m_pRef;
};

#define RetAddRef(p) ((p)->AddRef(), (p))
#define InlineAddRef(p) ((p)->AddRef(), (p))

template <class T>
class CBaseAutoPtr
{
  public:
    CBaseAutoPtr() : m_pObject(nullptr) {}
    CBaseAutoPtr(T* pFrom) : m_pObject(pFrom) {}

    operator const void*() const { return m_pObject; }
    operator void*() { return m_pObject; }
    operator const T*() const { return m_pObject; }
    operator const T*() { return m_pObject; }
    operator T*() { return m_pObject; }

    int operator=(int i)
    {
        assert(i == 0 && "Only NULL allowed on integer assign");
        m_pObject = nullptr;
        return 0;
    }
    T* operator=(T* p)
    {
        m_pObject = p;
        return p;
    }

    bool operator!() const { return !m_pObject; }
    bool operator!=(int i) const
    {
        assert(i == 0 && "Only NULL allowed on integer compare");
        return m_pObject != nullptr;
    }
    bool operator==(const void* p) const { return m_pObject == p; }
    bool operator!=(const void* p) const { return m_pObject != p; }
    bool operator==(T* p) const { return operator==(static_cast<void*>(p)); }
    bool operator!=(T* p) const { return operator!=(static_cast<void*>(p)); }
    bool operator==(const CBaseAutoPtr<T>& p) const { return operator==(static_cast<const void*>(p)); }
    bool operator!=(const CBaseAutoPtr<T>& p) const { return operator!=(static_cast<const void*>(p)); }

    T* operator->() { return m_pObject; }
    T& operator*() { return *m_pObject; }
    T** operator&() { return &m_pObject; }
    const T* operator->() const { return m_pObject; }
    const T& operator*() const { return *m_pObject; }
    T* const* operator&() const { return &m_pObject; }

  protected:
    CBaseAutoPtr(const CBaseAutoPtr<T>& from) : m_pObject(from.m_pObject) {}
    void operator=(const CBaseAutoPtr<T>& from) { m_pObject = from.m_pObject; }

    T* m_pObject;
};

template <class T>
class CRefPtr : public CBaseAutoPtr<T>
{
    using BaseClass = CBaseAutoPtr<T>;

  public:
    CRefPtr() = default;
    CRefPtr(T* pInit) : BaseClass(pInit) {}
    CRefPtr(const CRefPtr<T>& from) : BaseClass(from) {}
    ~CRefPtr()
    {
        if (BaseClass::m_pObject)
            BaseClass::m_pObject->Release();
    }

    void operator=(const CRefPtr<T>& from) { BaseClass::operator=(from); }
    int operator=(int i) { return BaseClass::operator=(i); }
    T* operator=(T* p) { return BaseClass::operator=(p); }
    operator bool() const { return !BaseClass::operator!(); }
    operator bool() { return !BaseClass::operator!(); }

    void SafeRelease()
    {
        if (BaseClass::m_pObject)
            BaseClass::m_pObject->Release();
        BaseClass::m_pObject = nullptr;
    }
    void AssignAddRef(T* pFrom)
    {
        if (pFrom)
            pFrom->AddRef();
        SafeRelease();
        BaseClass::m_pObject = pFrom;
    }
    void AddRefAssignTo(T*& pTo)
    {
        if (BaseClass::m_pObject)
            BaseClass::m_pObject->AddRef();
        ::SafeRelease(pTo);
        pTo = BaseClass::m_pObject;
    }
};

class CRefMT
{
  public:
    static int Increment(int* p) { return std::atomic_ref<int>(*p).fetch_add(1) + 1; }
    static int Decrement(int* p) { return std::atomic_ref<int>(*p).fetch_sub(1) - 1; }
};

class CRefST
{
  public:
    static int Increment(int* p) { return ++*p; }
    static int Decrement(int* p) { return --*p; }
};

template <bool bSelfDelete, typename CRefThreading = CRefMT>
class REFCOUNT_NOVTABLE CRefCountServiceBase
{
  protected:
    CRefCountServiceBase() : m_iRefs(1) {}
    virtual ~CRefCountServiceBase() = default;
    virtual bool OnFinalRelease() { return true; }

    int GetRefCount() const { return m_iRefs; }
    int DoAddRef() { return CRefThreading::Increment(&m_iRefs); }
    int DoRelease()
    {
        const int result = CRefThreading::Decrement(&m_iRefs);
        if (result)
            return result;
        if (OnFinalRelease() && bSelfDelete)
            delete this;
        return 0;
    }

  private:
    int m_iRefs;
};

class CRefCountServiceNull
{
  protected:
    static int DoAddRef() { return 1; }
    static int DoRelease() { return 1; }
};

template <typename CRefThreading = CRefMT>
class REFCOUNT_NOVTABLE CRefCountServiceDestruct
{
  protected:
    CRefCountServiceDestruct() : m_iRefs(1) {}
    virtual ~CRefCountServiceDestruct() = default;

    int GetRefCount() const { return m_iRefs; }
    int DoAddRef() { return CRefThreading::Increment(&m_iRefs); }
    int DoRelease()
    {
        const int result = CRefThreading::Decrement(&m_iRefs);
        if (result)
            return result;
        this->~CRefCountServiceDestruct();
        return 0;
    }

  private:
    int m_iRefs;
};

using CRefCountServiceST = CRefCountServiceBase<true, CRefST>;
using CRefCountServiceNoDeleteST = CRefCountServiceBase<false, CRefST>;
using CRefCountServiceMT = CRefCountServiceBase<true, CRefMT>;
using CRefCountServiceNoDeleteMT = CRefCountServiceBase<false, CRefMT>;
using CRefCountServiceNoDelete = CRefCountServiceNoDeleteMT;
using CRefCountService = CRefCountServiceMT;

template <class REFCOUNT_SERVICE = CRefCountService>
class REFCOUNT_NOVTABLE CRefCounted : public REFCOUNT_SERVICE
{
  public:
    ~CRefCounted() override = default;
    int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
    int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <class BASE1, class REFCOUNT_SERVICE = CRefCountService>
class REFCOUNT_NOVTABLE CRefCounted1 : public BASE1, public REFCOUNT_SERVICE
{
  public:
    ~CRefCounted1() override = default;
    int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
    int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <class BASE1, class BASE2, class REFCOUNT_SERVICE = CRefCountService>
class REFCOUNT_NOVTABLE CRefCounted2 : public BASE1, public BASE2, public REFCOUNT_SERVICE
{
  public:
    ~CRefCounted2() override = default;
    int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
    int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <class BASE1, class BASE2, class BASE3, class REFCOUNT_SERVICE = CRefCountService>
class REFCOUNT_NOVTABLE CRefCounted3 : public BASE1, public BASE2, public BASE3, public REFCOUNT_SERVICE
{
  public:
    ~CRefCounted3() override = default;
    int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
    int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <class BASE1, class BASE2, class BASE3, class BASE4, class REFCOUNT_SERVICE = CRefCountService>
class REFCOUNT_NOVTABLE CRefCounted4 : public BASE1, public BASE2, public BASE3, public BASE4,
                                      public REFCOUNT_SERVICE
{
  public:
    ~CRefCounted4() override = default;
    int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
    int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <class BASE1, class BASE2, class BASE3, class BASE4, class BASE5,
          class REFCOUNT_SERVICE = CRefCountService>
class REFCOUNT_NOVTABLE CRefCounted5 : public BASE1, public BASE2, public BASE3, public BASE4,
                                      public BASE5, public REFCOUNT_SERVICE
{
  public:
    ~CRefCounted5() override = default;
    int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
    int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

static_assert(sizeof(CRefCountService) == 0x10);
static_assert(sizeof(CRefCounted<>) == 0x10);

#undef REFCOUNT_NOVTABLE
