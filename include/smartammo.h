#pragma once

#include "engine/ehandle.h"
#include "tier1/utlvector.h"
#include <cstddef>
#include <cstdint>

class C_BaseEntity;
class CBaseEntity;
struct datamap_t;

class SmartAmmo_WeaponData_Client
{
  public:
    virtual datamap_t* GetPredDescMap();
    virtual const datamap_t* GetPredDescMap() const;

    int numTargetEntities;                                                           // 0x8
    CHandle<C_BaseEntity> targetEntities[8];                                         // 0xC
    float currentFrameSmartAmmoFractions[8];                                         // 0x2C
    float previousFractions[8];                                                      // 0x4C
    float currentFractions[8];                                                       // 0x6C
    int visiblePoints[8];                                                            // 0x8C
    float lastVisibleTimes[8];                                                       // 0xAC
    float lastFullLockTimes[8];                                                      // 0xCC
    CHandle<C_BaseEntity> storedTargets[8];                                          // 0xEC
    float lastNewTargetTime;                                                         // 0x10C
    int trackerCount;                                                                // 0x110
    CHandle<C_BaseEntity> trackerEntities[8];                                        // 0x114
    int trackerLocks[8];                                                             // 0x134
    float trackerTimes[8];                                                           // 0x154
    CUtlVectorFixed<CHandle<C_BaseEntity>, 8, std::int64_t> previousTargetEntities;  // 0x178
    CUtlVectorFixed<CHandle<C_BaseEntity>, 8, std::int64_t> previousTrackerEntities; // 0x1A0
    CUtlVectorFixed<int, 8, std::int64_t> previousTrackerLocks;                      // 0x1C8
};

class SmartAmmo_WeaponData
{
  public:
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }

    int numTargetEntities;                                                          // 0x08
    CHandle<CBaseEntity> targetEntities[8];                                         // 0x0C
    float currentFrameSmartAmmoFractions[8];                                        // 0x2C
    float previousFractions[8];                                                     // 0x4C
    float currentFractions[8];                                                      // 0x6C
    int visiblePoints[8];                                                           // 0x8C
    float lastVisibleTimes[8];                                                      // 0xAC
    float lastFullLockTimes[8];                                                     // 0xCC
    CHandle<CBaseEntity> storedTargets[8];                                          // 0xEC
    float lastNewTargetTime;                                                        // 0x10C
    int trackerCount;                                                               // 0x110
    CHandle<CBaseEntity> trackerEntities[8];                                        // 0x114
    int trackerLocks[8];                                                            // 0x134
    float trackerTimes[8];                                                          // 0x154
    CUtlVectorFixed<CHandle<CBaseEntity>, 8, std::int64_t> previousTargetEntities;  // 0x178
    CUtlVectorFixed<CHandle<CBaseEntity>, 8, std::int64_t> previousTrackerEntities; // 0x1A0
    CUtlVectorFixed<int, 8, std::int64_t> previousTrackerLocks;                     // 0x1C8
};

static_assert(sizeof(SmartAmmo_WeaponData) == 0x1F0);
