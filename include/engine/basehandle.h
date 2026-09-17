//========= Copyright Valve Corporation, All rights reserved. ============//
#pragma once

#include "tier0/dbg.h"
#include <cstdint>
#include <type_traits>

class IHandleEntity;

enum INVALID_EHANDLE_tag
{
    INVALID_EHANDLE
};

class CBaseHandle
{
    friend class CBaseEntityList;
    friend class C_BaseEntityList;

  public:
    constexpr CBaseHandle();
    constexpr CBaseHandle(INVALID_EHANDLE_tag);
    constexpr CBaseHandle(const CBaseHandle& other);
    explicit CBaseHandle(IHandleEntity* pEntity);
    CBaseHandle(int iEntry, int iSerialNumber);
    static constexpr CBaseHandle UnsafeFromIndex(int index);

    void Init(int iEntry, int iSerialNumber);
    void Term();

    constexpr bool IsValid() const;
    constexpr int GetEntryIndex() const;
    constexpr int GetSerialNumber() const;
    constexpr std::uint32_t ToInt() const;

    constexpr bool operator!=(const CBaseHandle& other) const;
    constexpr bool operator==(const CBaseHandle& other) const;
    constexpr bool operator<(const CBaseHandle& other) const;
    bool operator==(const IHandleEntity* pEntity) const;
    bool operator!=(const IHandleEntity* pEntity) const;
    bool operator<(const IHandleEntity* pEntity) const;

    const CBaseHandle& operator=(const IHandleEntity* pEntity);
    const CBaseHandle& Set(const IHandleEntity* pEntity);

    IHandleEntity* Get() const;

  protected:
    std::uint32_t m_Index;
};

//-----------------------------------------------------------------------------
// Inline implementation.
//-----------------------------------------------------------------------------
constexpr CBaseHandle::CBaseHandle() : m_Index(0xFFFFFFFFu)
{
}

constexpr CBaseHandle::CBaseHandle(INVALID_EHANDLE_tag) : CBaseHandle()
{
}

constexpr CBaseHandle::CBaseHandle(const CBaseHandle& other) : m_Index(other.m_Index)
{
}

inline CBaseHandle::CBaseHandle(IHandleEntity* pEntity)
{
    Set(pEntity);
}

inline CBaseHandle::CBaseHandle(int iEntry, int iSerialNumber)
{
    Init(iEntry, iSerialNumber);
}

constexpr CBaseHandle CBaseHandle::UnsafeFromIndex(int index)
{
    CBaseHandle handle;
    handle.m_Index = static_cast<std::uint32_t>(index);
    return handle;
}

inline void CBaseHandle::Init(int iEntry, int iSerialNumber)
{
    Assert(iEntry >= 0 && iEntry < 0x4000);
    Assert(iSerialNumber >= 0 && iSerialNumber <= 0xFFFF);
    m_Index = static_cast<std::uint32_t>(iEntry) | (static_cast<std::uint32_t>(iSerialNumber) << 16);
}

inline void CBaseHandle::Term()
{
    m_Index = 0xFFFFFFFFu;
}

constexpr bool CBaseHandle::IsValid() const
{
    return m_Index != 0xFFFFFFFFu;
}

constexpr int CBaseHandle::GetEntryIndex() const
{
    return IsValid() ? static_cast<int>(m_Index & 0xFFFFu) : 0x3FFF;
}

constexpr int CBaseHandle::GetSerialNumber() const
{
    return static_cast<int>(m_Index >> 16);
}

constexpr std::uint32_t CBaseHandle::ToInt() const
{
    return m_Index;
}

constexpr bool CBaseHandle::operator!=(const CBaseHandle& other) const
{
    return m_Index != other.m_Index;
}

constexpr bool CBaseHandle::operator==(const CBaseHandle& other) const
{
    return m_Index == other.m_Index;
}

constexpr bool CBaseHandle::operator<(const CBaseHandle& other) const
{
    return m_Index < other.m_Index;
}

inline bool CBaseHandle::operator==(const IHandleEntity* pEntity) const
{
    return Get() == pEntity;
}

inline bool CBaseHandle::operator!=(const IHandleEntity* pEntity) const
{
    return Get() != pEntity;
}

inline const CBaseHandle& CBaseHandle::operator=(const IHandleEntity* pEntity)
{
    return Set(pEntity);
}
