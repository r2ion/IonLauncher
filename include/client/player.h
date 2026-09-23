#pragma once

#include "client/basecombatcharacter.h"

class C_Player : public C_BaseCombatCharacter
{
  public:
    static C_Player* GetLocalViewPlayer(int splitScreenSlot = -1);
    static C_Player* GetLocalPlayer(int splitScreenSlot = -1);

    bool IsMantling() const;
};
