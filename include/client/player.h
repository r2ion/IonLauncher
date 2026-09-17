#pragma once

#include "client/basecombatcharacter.h"

class C_Player : public C_BaseCombatCharacter
{
  public:
    bool IsMantling() const;
};

