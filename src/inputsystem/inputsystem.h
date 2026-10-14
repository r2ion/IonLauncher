#pragma once

#include "inputsystem/InputEnums.h"

class CInputSystem
{
public:
    void PostEvent(InputEventType_t type, int tick, int data, int data2, int data3);
};
