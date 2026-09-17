//========= Copyright Valve Corporation, All rights reserved. ============//
#pragma once

#include "mathlib/mathlib.h"
#include <cstddef>

class C_BaseAnimating;

class C_BoneAccessor
{
  public:
    C_BoneAccessor()
        : m_pAnimating(nullptr), m_ReadableBones(0), m_WritableBones(0), m_numBones(0), m_pBones(nullptr)
    {
    }

    C_BoneAccessor(matrix3x4a_t* pBones)
        : m_pAnimating(nullptr), m_ReadableBones(-1), m_WritableBones(-1), m_numBones(0), m_pBones(pBones)
    {
    }

    void Init(const C_BaseAnimating* pAnimating, matrix3x4a_t* pBones);
    void Init(const C_BaseAnimating* pAnimating, matrix3x4a_t* pBones, int numBones)
    {
        m_pAnimating = const_cast<C_BaseAnimating*>(pAnimating);
        m_numBones = numBones;
        m_pBones = pBones;
    }

    int GetReadableBones() const { return m_ReadableBones; }
    void SetReadableBones(int flags) { m_ReadableBones = flags; }
    int GetWritableBones() const { return m_WritableBones; }
    void SetWritableBones(int flags) { m_WritableBones = flags; }

    const matrix3x4a_t& GetBone(int iBone) const
    {
#ifdef _DEBUG
        SanityCheckBone(iBone, true);
#endif
        return m_pBones[iBone];
    }

    const matrix3x4a_t& operator[](int iBone) const { return GetBone(iBone); }

    matrix3x4a_t& GetBoneForWrite(int iBone)
    {
#ifdef _DEBUG
        SanityCheckBone(iBone, false);
#endif
        return m_pBones[iBone];
    }

    matrix3x4a_t* GetBoneArrayForWrite() const { return m_pBones; }

    C_BaseAnimating* m_pAnimating;
    int m_ReadableBones;
    int m_WritableBones;
    int m_numBones;
    matrix3x4a_t* m_pBones;

  private:
#ifdef _DEBUG
    void SanityCheckBone(int iBone, bool bReadable) const;
#endif
};

static_assert(sizeof(C_BoneAccessor) == 0x20);
static_assert(offsetof(C_BoneAccessor, m_pAnimating) == 0x0);
static_assert(offsetof(C_BoneAccessor, m_ReadableBones) == 0x8);
static_assert(offsetof(C_BoneAccessor, m_WritableBones) == 0xC);
static_assert(offsetof(C_BoneAccessor, m_numBones) == 0x10);
static_assert(offsetof(C_BoneAccessor, m_pBones) == 0x18);
