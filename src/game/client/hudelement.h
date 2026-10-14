#pragma once

#include "tier1/utlvector.h"

#include <array>
#include <cstddef>

class CHud;

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

