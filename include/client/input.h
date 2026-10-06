#pragma once

#include "game/client/kbutton.h"
#include "inputsystem/ButtonCode.h"

#include <cstddef>

class C_BaseCombatWeapon;
class CKeyboardKey;
class CUserCmd;
class QAngle;
class Vector3D;
class bf_read;
class bf_write;
struct CameraThirdData_t;
struct ClientVerifiedUserCmd;

enum KeyBindType_t : int;
enum UiInputDevice_e : int;

class IInput
{
  public:
    virtual ~IInput() = default;                                                                                                      // 0
    virtual void Init_All() = 0;                                                                                                     // 1
    virtual void Shutdown_All() = 0;                                                                                                 // 2
    virtual int GetButtonBits(bool resetState) = 0;                                                                                   // 3
    virtual void CreateMove(int sequenceNumber, float inputSampleFrameTime, bool active) = 0;                                         // 4
    virtual void ExtraMouseSample(float inputSampleFrameTime) = 0;                                                                    // 5
    virtual bool WriteUsercmdDeltaToBuffer(int splitScreenSlot, bf_write* buffer, int from, int to) = 0;                              // 6
    virtual void EncodeUserCmdToBuffer(int splitScreenSlot, bf_write* buffer, int commandNumber) = 0;                                 // 7
    virtual void DecodeUserCmdFromBuffer(int splitScreenSlot, bf_read* buffer, int commandNumber) = 0;                                // 8
    virtual CUserCmd* GetUserCmd(int splitScreenSlot, int sequenceNumber) = 0;                                                         // 9
    virtual void MakeWeaponSelection(C_BaseCombatWeapon* weapon) = 0;                                                                 // 10
    virtual void UpdateCycleWeapon() = 0;                                                                                            // 11
    virtual void SetOffhandQuickSelect() = 0;                                                                                        // 12
    virtual bool IsMousePresent() = 0;                                                                                               // 13
    virtual float KeyState(kbutton_t* key) = 0;                                                                                       // 14
    virtual void PreKeyEvent(bool down, ButtonCode_t keyCode) = 0;                                                                    // 15
    virtual int KeyEvent(bool down, ButtonCode_t keyCode, const char* currentBinding) = 0;                                            // 16
    virtual kbutton_t* FindKey(const char* name) = 0;                                                                                 // 17
    virtual bool ControllerModeActive() = 0;                                                                                          // 18
    virtual UiInputDevice_e GetUIInputDeviceType() = 0;                                                                               // 19
    virtual void Joystick_SetSampleTime(float frameTime) = 0;                                                                         // 20
    virtual void IN_SetSampleTime(float frameTime) = 0;                                                                               // 21
    virtual void AccumulateMouse() = 0;                                                                                               // 22
    virtual void ActivateMouse() = 0;                                                                                                 // 23
    virtual void DeactivateMouse() = 0;                                                                                               // 24
    virtual void ClearStates() = 0;                                                                                                   // 25
    virtual float GetLookSpring() = 0;                                                                                                // 26
    virtual void GetFullscreenMousePos(int* x, int* y, int* unclampedX, int* unclampedY) = 0;                                         // 27
    virtual void SetFullscreenMousePos(int x, int y) = 0;                                                                             // 28
    virtual void ResetMouse() = 0;                                                                                                    // 29
    virtual float GetLastForwardMove() = 0;                                                                                            // 30
    virtual void CAM_Think() = 0;                                                                                                     // 31
    virtual bool CAM_IsThirdPerson(int splitScreenSlot = -1) = 0;                                                                     // 32
    virtual void CAM_GetCameraOffset(Vector3D& offset) = 0;                                                                           // 33
    virtual void CAM_ToThirdPerson() = 0;                                                                                             // 34
    virtual void CAM_ToFirstPerson() = 0;                                                                                             // 35
    virtual void CAM_ToThirdPersonShoulder() = 0;                                                                                     // 36
    virtual void CAM_StartMouseMove() = 0;                                                                                            // 37
    virtual void CAM_EndMouseMove() = 0;                                                                                              // 38
    virtual void CAM_StartDistance() = 0;                                                                                             // 39
    virtual void CAM_EndDistance() = 0;                                                                                               // 40
    virtual int CAM_InterceptingMouse() = 0;                                                                                          // 41
    virtual void CAM_Command(int command) = 0;                                                                                        // 42
    virtual void CAM_ToOrthographic() = 0;                                                                                            // 43
    virtual bool CAM_IsOrthographic() const = 0;                                                                                      // 44
    virtual void CAM_OrthographicSize(float& width, float& height) const = 0;                                                         // 45
    virtual void AddIKGroundContactInfo(int entityIndex, float minHeight, float maxHeight) = 0;                                       // 46
    virtual void LevelInit() = 0;                                                                                                     // 47
    virtual void ClearInputButton(int bits) = 0;                                                                                      // 48
    virtual void CAM_SetCameraThirdData(CameraThirdData_t* cameraData, const QAngle& cameraOffset) = 0;                               // 49
    virtual void CAM_CameraThirdThink() = 0;                                                                                          // 50
    virtual void SetAbilityBinding(int splitScreenSlot, int abilityIndex, const char* downBinding, const char* upBinding) = 0;        // 51
    virtual const char* GetAbilityDownBinding(int splitScreenSlot, int abilityIndex) = 0;                                             // 52
    virtual const char* GetAbilityUpBinding(int splitScreenSlot, int abilityIndex) = 0;                                               // 53
    virtual int GetAbilityBindingCount() = 0;                                                                                         // 54
    virtual int GetAbilityBindingMaxLen() = 0;                                                                                        // 55
    virtual void ExecuteAbilityBinding(int splitScreenSlot, int abilityIndex, KeyBindType_t bindType) = 0;                            // 56
    virtual float GetLatestSidearmSwapTime() = 0;                                                                                     // 57
    virtual void SetLatestSidearmSwapTime(float time) = 0;                                                                            // 58
    virtual float GetLatestOrdnanceSwapTime() = 0;                                                                                    // 59
    virtual void SetLatestOrdnanceSwapTime(float time) = 0;                                                                           // 60
    virtual float GetSidearmQuickSelectPressTime() = 0;                                                                               // 61
    virtual void SetSidearmQuickSelectPressTime(float time) = 0;                                                                      // 62
    virtual float GetOffhandQuickSelectPressTime() = 0;                                                                               // 63
    virtual void SetOffhandQuickSelectPressTime(float time) = 0;                                                                      // 64
    virtual void SetButtonPairLink(int firstButton, int secondButton, int thirdButton) = 0;                                           // 65
    virtual void SetControllerMode(bool controllerMode) = 0;                                                                          // 66
};

static_assert(sizeof(IInput) == sizeof(void*));

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
