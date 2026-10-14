#pragma once

#include "game/client/iinput.h"

#include <cstddef>

class CInput : public IInput
{
  public:
    struct PerUserInput_t
    {
        std::byte m_Reserved0000[0x8];
        float m_flRemainingJoystickSampleTime;
        float m_flKeyboardSampleTime;
        std::byte m_Reserved0010[0xC];
        float m_flLatestSidearmSwapTime;
        float m_flLatestOrdnanceSwapTime;
        float m_flSidearmQuickSelectPressTime;
        float m_flOffhandQuickSelectPressTime;
        std::byte m_Reserved002C[0xB4];
        CUserCmd* m_pCommands;
        ClientVerifiedUserCmd* m_pVerifiedCommands;
        int m_hSelectedWeapon;
        bool m_bUpdateCycleWeapon;
        bool m_bSetOffhandQuickSelect;
        std::byte m_Padding00F6[2];
        CameraThirdData_t* m_pCameraThirdData;
        std::byte m_Reserved0100[0x580];
    };

    void SetInputSampleTime(float frameTime);
    void ResetExtraMouseSamples();
    float GetExtraMouseSampleTime() const;

    bool m_fMouseInitialized;
    bool m_fMouseActive;
    bool m_joystickIsInitialized;
    bool m_bControllerMode;
    float m_fAccumulatedMouseMove;
    CKeyboardKey* m_pKeys;
    PerUserInput_t m_PerUser[1];
    void* m_hInputContext;
    void* m_hInactiveAppDummyContext;
    std::byte m_IKContactPointMutex[0x28];
};

static_assert(sizeof(CInput::PerUserInput_t) == 0x680);
static_assert(offsetof(CInput::PerUserInput_t, m_flRemainingJoystickSampleTime) == 0x8);
static_assert(offsetof(CInput::PerUserInput_t, m_flKeyboardSampleTime) == 0xC);
static_assert(offsetof(CInput::PerUserInput_t, m_flLatestSidearmSwapTime) == 0x1C);
static_assert(offsetof(CInput::PerUserInput_t, m_flLatestOrdnanceSwapTime) == 0x20);
static_assert(offsetof(CInput::PerUserInput_t, m_flSidearmQuickSelectPressTime) == 0x24);
static_assert(offsetof(CInput::PerUserInput_t, m_flOffhandQuickSelectPressTime) == 0x28);
static_assert(offsetof(CInput::PerUserInput_t, m_pCommands) == 0xE0);
static_assert(offsetof(CInput::PerUserInput_t, m_pVerifiedCommands) == 0xE8);
static_assert(offsetof(CInput::PerUserInput_t, m_hSelectedWeapon) == 0xF0);
static_assert(offsetof(CInput::PerUserInput_t, m_bUpdateCycleWeapon) == 0xF4);
static_assert(offsetof(CInput::PerUserInput_t, m_bSetOffhandQuickSelect) == 0xF5);
static_assert(offsetof(CInput::PerUserInput_t, m_pCameraThirdData) == 0xF8);

static_assert(offsetof(CInput, m_fMouseInitialized) == 0x8);
static_assert(offsetof(CInput, m_fMouseActive) == 0x9);
static_assert(offsetof(CInput, m_joystickIsInitialized) == 0xA);
static_assert(offsetof(CInput, m_bControllerMode) == 0xB);
static_assert(offsetof(CInput, m_fAccumulatedMouseMove) == 0xC);
static_assert(offsetof(CInput, m_pKeys) == 0x10);
static_assert(offsetof(CInput, m_PerUser) == 0x18);
static_assert(offsetof(CInput, m_hInputContext) == 0x698);
static_assert(offsetof(CInput, m_hInactiveAppDummyContext) == 0x6A0);
static_assert(offsetof(CInput, m_IKContactPointMutex) == 0x6A8);
static_assert(sizeof(CInput) == 0x6D0);

extern CInput* g_pInput;
