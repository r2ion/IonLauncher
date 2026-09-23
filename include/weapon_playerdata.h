#pragma once

#include "engine/ehandle.h"
#include <cstddef>

class C_BaseEntity;
class CBaseEntity;
struct datamap_t;
struct ServerDataMap;

class WeaponPlayerData_Client
{
  public:
    virtual datamap_t* GetPredDescMap();
    virtual const datamap_t* GetPredDescMap() const;

    float GetTargetZoomFOV() const
    {
        return m_targetZoomFOV;
    }

    float GetZoomFOVInterpAmount(float currentTime) const
    {
        if (m_zoomFOVLerpTime <= 0.0f)
            return 1.0f;
        const float amount = (currentTime - (m_zoomFOVLerpEndTime - m_zoomFOVLerpTime)) / m_zoomFOVLerpTime;
        const float fraction = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
        return (3.0f - (fraction + fraction)) * (fraction * fraction);
    }

    float m_spread;                                   // 0x8
    float m_spreadStartTime;                          // 0xC
    float m_spreadStartFracHip;                       // 0x10
    float m_spreadStartFracADS;                       // 0x14
    float m_kickSpreadHipfire;                        // 0x18
    float m_kickSpreadADS;                            // 0x1C
    float m_kickTime;                                 // 0x20
    float m_kickScaleBase;                            // 0x24
    float m_semiAutoTriggerHoldTime;                  // 0x28
    bool m_semiAutoTriggerDown;                       // 0x2C
    bool m_pendingTriggerPull;                        // 0x2D
    bool m_semiAutoNeedsRechamber;                    // 0x2E
    bool m_pendingReloadAttempt;                      // 0x2F
    bool m_offhandHybridNormalMode;                   // 0x30
    bool m_pendingoffhandHybridToss;                  // 0x31
    bool m_fastHolster;                               // 0x32
    bool m_didFirstDeploy;                            // 0x33
    bool m_shouldCatch;                               // 0x34
    bool m_clipModelIsHidden;                         // 0x35
    bool m_customActivityPlayRaiseOnComplete;         // 0x36
    bool m_segmentedReloadEndSeqRequired;             // 0x37
    bool m_segmentedReloadStartedEmpty;               // 0x38
    bool m_segmentedReloadStartedOneHanded;           // 0x39
    bool m_segmentedReloadCanRestartLoop;             // 0x3A
    bool m_segmentedReloadLoopFireLocked;             // 0x3B
    int m_customActivityAttachedModelIndex;           // 0x3C
    int m_customActivityAttachedModelAttachmentIndex; // 0x40
    float m_fireRateLerp_startTime;                   // 0x44
    float m_fireRateLerp_startFraction;               // 0x48
    float m_fireRateLerp_stopTime;                    // 0x4C
    float m_fireRateLerp_stopFraction;                // 0x50
    int m_chargeAnimIndex;                            // 0x54
    int m_chargeAnimIndexOld;                         // 0x58
    CHandle<C_BaseEntity> m_proScreen_owner;          // 0x5C
    int m_proScreen_int0;                             // 0x60
    int m_proScreen_int1;                             // 0x64
    int m_proScreen_int2;                             // 0x68
    float m_proScreen_float0;                         // 0x6C
    float m_proScreen_float1;                         // 0x70
    float m_proScreen_float2;                         // 0x74
    int m_reloadMilestone;                            // 0x78
    float m_fullReloadStartTime;                      // 0x7C
    float m_scriptTime0;                              // 0x80
    int m_scriptFlags0;                               // 0x84
    float m_curZoomFOV;                               // 0x88
    float m_targetZoomFOV;                            // 0x8C
    float m_zoomFOVLerpTime;                          // 0x90
    float m_zoomFOVLerpEndTime;                       // 0x94
    float m_latestDryfireTime;                        // 0x98
    float m_lastRequestedAttackTime;                  // 0x9C
    int m_currentAltFireAnimIndex;                    // 0xA0
};

class WeaponPlayerData
{
  public:
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    virtual ServerDataMap* GetDataDescMap() { return nullptr; }
    float GetTargetZoomFOV() const
    {
        return m_targetZoomFOV;
    }

    float GetZoomFOVInterpAmount(float currentTime) const
    {
        if (m_zoomFOVLerpTime <= 0.0f)
            return 1.0f;
        const float amount = (currentTime - (m_zoomFOVLerpEndTime - m_zoomFOVLerpTime)) / m_zoomFOVLerpTime;
        const float fraction = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
        return (3.0f - (fraction + fraction)) * (fraction * fraction);
    }

    float m_spread;                                   // 0x08
    float m_spreadStartTime;                          // 0x0C
    float m_spreadStartFracHip;                       // 0x10
    float m_spreadStartFracADS;                       // 0x14
    float m_kickSpreadHipfire;                        // 0x18
    float m_kickSpreadADS;                            // 0x1C
    float m_kickTime;                                 // 0x20
    float m_kickScaleBase;                            // 0x24
    float m_semiAutoTriggerHoldTime;                  // 0x28
    bool m_semiAutoTriggerDown;                       // 0x2C
    bool m_pendingTriggerPull;                        // 0x2D
    bool m_semiAutoNeedsRechamber;                    // 0x2E
    bool m_pendingReloadAttempt;                      // 0x2F
    bool m_offhandHybridNormalMode;                   // 0x30
    bool m_pendingoffhandHybridToss;                  // 0x31
    bool m_fastHolster;                               // 0x32
    bool m_didFirstDeploy;                            // 0x33
    bool m_shouldCatch;                               // 0x34
    bool m_clipModelIsHidden;                         // 0x35
    bool m_customActivityPlayRaiseOnComplete;         // 0x36
    bool m_segmentedReloadEndSeqRequired;             // 0x37
    bool m_segmentedReloadStartedEmpty;               // 0x38
    bool m_segmentedReloadStartedOneHanded;           // 0x39
    bool m_segmentedReloadCanRestartLoop;             // 0x3A
    bool m_segmentedReloadLoopFireLocked;             // 0x3B
    int m_customActivityAttachedModelIndex;           // 0x3C
    int m_customActivityAttachedModelAttachmentIndex; // 0x40
    float m_fireRateLerp_startTime;                   // 0x44
    float m_fireRateLerp_startFraction;               // 0x48
    float m_fireRateLerp_stopTime;                    // 0x4C
    float m_fireRateLerp_stopFraction;                // 0x50
    int m_chargeAnimIndex;                            // 0x54
    int m_chargeAnimIndexOld;                         // 0x58
    CHandle<CBaseEntity> m_proScreen_owner;           // 0x5C
    int m_proScreen_int0;                             // 0x60
    int m_proScreen_int1;                             // 0x64
    int m_proScreen_int2;                             // 0x68
    float m_proScreen_float0;                         // 0x6C
    float m_proScreen_float1;                         // 0x70
    float m_proScreen_float2;                         // 0x74
    int m_reloadMilestone;                            // 0x78
    float m_fullReloadStartTime;                      // 0x7C
    float m_scriptTime0;                              // 0x80
    int m_scriptFlags0;                               // 0x84
    float m_curZoomFOV;                               // 0x88
    float m_targetZoomFOV;                            // 0x8C
    float m_zoomFOVLerpTime;                          // 0x90
    float m_zoomFOVLerpEndTime;                       // 0x94
    float m_latestDryfireTime;                        // 0x98
    float m_lastRequestedAttackTime;                  // 0x9C
    int m_currentAltFireAnimIndex;                    // 0xA0
};

static_assert(sizeof(WeaponPlayerData) == 0xA8);
