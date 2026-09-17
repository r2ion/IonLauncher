//========= Copyright Valve Corporation, All rights reserved. ============//
#include "client/bone_accessor.h"
#include "client/baseanimating.h"
#include "studio.h"

void C_BoneAccessor::Init(const C_BaseAnimating* pAnimating, matrix3x4a_t* pBones)
{
    const CStudioHdr* hdr = pAnimating ? pAnimating->GetModelPtr() : nullptr;
    Init(pAnimating, pBones, hdr ? hdr->numbones() : 0);
}

#ifdef _DEBUG
void C_BoneAccessor::SanityCheckBone(int iBone, bool bReadable) const
{
    if (m_pAnimating)
    {
        const CStudioHdr* hdr = m_pAnimating->GetModelPtr();
        if (hdr)
        {
            Assert(iBone >= 0 && iBone < m_numBones);
            const int flags = hdr->boneFlags(iBone);
            if (bReadable)
                AssertOnce(flags & m_ReadableBones);
            else
                AssertOnce(flags & m_WritableBones);
        }
    }
}
#endif
