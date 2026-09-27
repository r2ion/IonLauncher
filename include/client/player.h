#pragma once

#include "client/basecombatcharacter.h"
#include "game/shared/usercmd.h"

#include <cstddef>

static_assert(sizeof(C_BaseCombatCharacter) == 0x1230);

class C_Player : public C_BaseCombatCharacter
{
  public:
    static C_Player* GetLocalViewPlayer(int splitScreenSlot = -1);
    static C_Player* GetLocalPlayer(int splitScreenSlot = -1);

    bool IsMantling() const;

    std::byte m_Padding1230[0x16D0];
    CUserCmd m_LastCmd;
    CHandle<C_BaseEntity> m_hCockpitProp;
    CHandle<C_BaseEntity> m_usePromptEntity;
    int m_afButtonForced;
    std::byte m_Padding2A54[4];
    CUserCmd* m_pCurrentCommand;
};
