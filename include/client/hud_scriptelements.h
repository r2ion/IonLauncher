#pragma once

#include "common/remote_functions.h"
#include "tier1/bitbuf.h"
#include "tier1/utlvector.h"
#include "vgui_controls/EditablePanel.h"
#include "vscript/ivscript.h"
#include "vscript/languages/squirrel_re/include/squirrel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

class CHud;
class CClientScriptHud;
class ConVar;

class CHudElement
{
  public:
    CHudElement(const char* elementName);
    virtual ~CHudElement();

    virtual void SetHud(CHud* hud);
    virtual void Init();
    virtual void VidInit();
    virtual void LevelInit();
    virtual void LevelShutdown();
    virtual void Reset();
    virtual void ProcessInput();
    virtual const char* GetName() const;
    virtual bool ShouldDraw();
    virtual bool IsActive();
    virtual void SetActive(bool active);
    virtual void SetHiddenBits(int hiddenBits);
    virtual bool WantsHudLayoutEntry();
    virtual int GetRenderGroupPriority();
    virtual void OnSplitScreenStateChanged();

  protected:
    bool m_bActive;                                    // 0x08
    std::array<std::byte, 3> m_ActivePadding;          // 0x09
    int m_iHiddenBits;                                 // 0x0C
    int m_nSplitScreenPlayerSlot;                      // 0x10
    std::array<std::byte, 4> m_SplitScreenSlotPadding; // 0x14
    const char* m_pElementName;                        // 0x18
    bool m_bNeedsRemove;                               // 0x20
    bool m_bIsParentedToClientDLLRootPanel;            // 0x21
    std::array<std::byte, 6> m_ParentingPadding;       // 0x22
    CUtlVector<int> m_HudRenderGroups;                 // 0x28
    CHud* m_pHud;                                      // 0x48
};

static_assert(sizeof(CHudElement) == 0x50);

struct C_BaseScriptRemoteFunctionCall
{
    static constexpr std::size_t MAX_PARAMETERS = MAX_SCRIPT_REMOTE_FUNCTION_PARAMETERS;

    std::uint64_t m_FunctionIndex;
    std::uint64_t m_ParameterCount;
    bool m_CallFromUI;
    bool m_Replay;
    std::array<std::byte, 6> m_HeaderPadding;
    std::array<ScriptVariant_t, MAX_PARAMETERS> m_Parameters;
};

struct C_CBaseScriptRemoteFunctionCallReceiveQueue
{
    static constexpr std::size_t CAPACITY = 1024;

    std::uint32_t m_ReadIndex;
    std::uint32_t m_WriteIndex;
    std::array<std::int32_t, CAPACITY> m_SnapshotTicks;
    std::array<C_BaseScriptRemoteFunctionCall, CAPACITY> m_Calls;
};

struct ScriptRemoteFunctionRegistrationState
{
    bool m_RegistrationStarted;
    bool m_RegistrationFinished;
    std::array<std::byte, 2> m_Padding;
    std::uint32_t m_Checksum;
};

struct ClientRemoteFunctionNameHash
{
    using is_transparent = void;

    std::size_t operator()(std::string_view name) const noexcept;
};

using RegisteredClientRemoteFunctions = std::unordered_map<std::string, bool, ClientRemoteFunctionNameHash, std::equal_to<>>;

enum class ClientRemoteFunctionRegistrationState
{
    Init,
    Registering,
    Done,
};

struct NamedScriptRemoteFunctionCall
{
    const std::string* m_FunctionName = nullptr;
    std::uint32_t m_ParameterCount = 0;
    bool m_CallFromUI = false;
    bool m_Replay = false;
    std::array<ScriptVariant_t, MAX_SCRIPT_REMOTE_FUNCTION_PARAMETERS> m_Parameters;
};

struct HudScriptElementsRemoteFunctionState
{
    using MessageCallback = void (*)(bf_read&);

    static constexpr std::size_t CALL_QUEUE_CAPACITY = C_CBaseScriptRemoteFunctionCallReceiveQueue::CAPACITY;

    MessageCallback m_RemoteFunctionCallCallback = nullptr;
    MessageCallback m_RemoteFunctionCallsChecksumCallback = nullptr;
    ScriptRemoteFunctionRegistrationState* m_NativeRegistrationState = nullptr;
    C_CBaseScriptRemoteFunctionCallReceiveQueue* m_NativeCallQueue = nullptr;
    ConVar* m_KillReplayPlayNonReplayRemoteCallsOnLocalClientPlayer = nullptr;

    RegisteredClientRemoteFunctions m_Functions;
    ClientRemoteFunctionRegistrationState m_RegistrationState = ClientRemoteFunctionRegistrationState::Init;

    std::uint32_t m_NamedCallReadIndex = 0;
    std::uint32_t m_NamedCallWriteIndex = 0;
    std::array<std::int32_t, CALL_QUEUE_CAPACITY> m_NamedCallSnapshotTicks{};
    std::array<NamedScriptRemoteFunctionCall, CALL_QUEUE_CAPACITY> m_NamedCalls;
};

class CHudScriptElements : public CHudElement, public vgui::EditablePanel
{
  public:
    CHudScriptElements(const char* elementName);
    ~CHudScriptElements() override;

    vgui::PanelMessageMap* GetMessageMap() override;
    vgui::PanelAnimationMap* GetAnimMap() override;
    vgui::PanelKeyBindingMap* GetKBMap() override;
    void ApplySchemeSettings(vgui::IScheme* scheme) override;
    void Paint() override;

    void Init() override;
    void LevelInit() override;
    void LevelShutdown() override;
    bool ShouldDraw() override;

    static const char* GetPanelClassName();
    static void* GetVar_m_flDeltaLifetime(vgui::Panel* panel);

    static void InitializeRemoteFunctions(HudScriptElementsRemoteFunctionState::MessageCallback remoteFunctionCallCallback,
                                          HudScriptElementsRemoteFunctionState::MessageCallback remoteFunctionCallsChecksumCallback,
                                          ScriptRemoteFunctionRegistrationState* nativeRegistrationState,
                                          C_CBaseScriptRemoteFunctionCallReceiveQueue* nativeCallQueue,
                                          ConVar* killReplayPlayNonReplayRemoteCallsOnLocalClientPlayer);

    static void BeginRemoteFunctionRegistration();
    static void FinishRemoteFunctionRegistration();
    static SQRESULT RegisterRemoteFunction(HSQUIRRELVM sqvm, bool extended, bool& callOriginal);
    static void ExecuteNamedRemoteFunctionCalls(std::int32_t tick);

    void ReloadControlSettings();
    void ExecHUDScriptCallback();
    void MsgFunc_RemoteFunctionCall(bf_read* message);
    void MsgFunc_NamedRemoteFunctionCall(bf_read* message);
    void MsgFunc_RemoteFunctionCallsChecksum(bf_read* message);

  private:
    static const std::set<std::string> s_VanillaRemoteFunctions;
    static HudScriptElementsRemoteFunctionState s_RemoteFunctions;

    struct CHudScriptElements_RegisterMap
    {
    };
    struct CHudScriptElements_Register
    {
    };
    struct CHudScriptElements_RegisterKBMap
    {
    };
    struct PanelAnimationVar_m_flDeltaLifetime
    {
    };

    CHudScriptElements_RegisterMap m_RegisterClass;                   // 0x308
    CHudScriptElements_Register m_RegisterAnimationClass;             // 0x309
    CHudScriptElements_RegisterKBMap m_RegisterClassKB;               // 0x30A
    PanelAnimationVar_m_flDeltaLifetime m_m_flDeltaLifetime_register; // 0x30B
    float m_flDeltaLifetime;                                          // 0x30C
    std::uint32_t m_RemoteFunctionCallsChecksum;                      // 0x310
    bool m_RemoteFunctionCallsChecksumReceived;                       // 0x314
    std::array<std::byte, 3> m_RemoteFunctionChecksumPadding;         // 0x315
    CClientScriptHud* m_clientScriptHud;                              // 0x318
};

CHudScriptElements* Create_CHudScriptElements();
