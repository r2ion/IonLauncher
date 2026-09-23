//========= Copyright Valve Corporation, All rights reserved. ============//
#pragma once

#include "engine/basehandle.h"

using EHANDLE = CBaseHandle;

template <class T> class CHandle : public CBaseHandle
{
  public:
    CHandle();
    CHandle(INVALID_EHANDLE_tag);
    CHandle(int iEntry, int iSerialNumber);
    CHandle(T* pEntity);

    static CHandle UnsafeFromBaseHandle(const CBaseHandle& handle);
    static CHandle UnsafeFromIndex(int index);

    bool ChangedFrom(T* pEntity) const;
    T* Get() const;
    void Set(const T* pEntity);

    operator T*();
    operator T*() const;
    bool operator!() const;
    bool operator==(T* pEntity) const;
    bool operator!=(T* pEntity) const;
    CHandle& operator=(const T* pEntity);
    T* operator->() const;
};

template <class T> inline CHandle<T>::CHandle() = default;

template <class T> inline CHandle<T>::CHandle(INVALID_EHANDLE_tag) : CBaseHandle(INVALID_EHANDLE)
{
}

template <class T> inline CHandle<T>::CHandle(int iEntry, int iSerialNumber) : CBaseHandle(iEntry, iSerialNumber)
{
}

template <class T> inline CHandle<T>::CHandle(T* pEntity)
{
    Set(pEntity);
}

template <class T> inline CHandle<T> CHandle<T>::UnsafeFromBaseHandle(const CBaseHandle& handle)
{
    CHandle result;
    result.m_Index = handle.ToInt();
    return result;
}

template <class T> inline CHandle<T> CHandle<T>::UnsafeFromIndex(int index)
{
    CHandle result;
    result.m_Index = static_cast<std::uint32_t>(index);
    return result;
}

template <class T> inline bool CHandle<T>::ChangedFrom(T* pEntity) const
{
    return pEntity ? Get() != pEntity : IsValid();
}

template <class T> inline T* CHandle<T>::Get() const
{
    return reinterpret_cast<T*>(CBaseHandle::Get());
}

template <class T> inline void CHandle<T>::Set(const T* pEntity)
{
    CBaseHandle::Set(reinterpret_cast<const IHandleEntity*>(pEntity));
}

template <class T> inline CHandle<T>::operator T*()
{
    return Get();
}

template <class T> inline CHandle<T>::operator T*() const
{
    return Get();
}

template <class T> inline bool CHandle<T>::operator!() const
{
    return !Get();
}

template <class T> inline bool CHandle<T>::operator==(T* pEntity) const
{
    return Get() == pEntity;
}

template <class T> inline bool CHandle<T>::operator!=(T* pEntity) const
{
    return Get() != pEntity;
}

template <class T> inline CHandle<T>& CHandle<T>::operator=(const T* pEntity)
{
    Set(pEntity);
    return *this;
}

template <class T> inline T* CHandle<T>::operator->() const
{
    return Get();
}

template <typename T> FORCEINLINE void EnsureValidValue(CHandle<T>& handle)
{
    handle = INVALID_EHANDLE;
}
