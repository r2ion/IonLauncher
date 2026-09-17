//========= Copyright Valve Corporation, All rights reserved. ============//
// Source SDK 2013 particle_parse.h declarations with the retail R2 attachment ABI.
#pragma once

#include "mathlib/vector.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"

class C_BaseEntity;
class IFileList;

// Retail client.dll: script constants at 0x1D31E0 and attachment dispatch at 0x27E0E0.
enum ParticleAttachment_t : int
{
    PATTACH_ABSORIGIN = 0,
    PATTACH_ABSORIGIN_FOLLOW = 1,
    PATTACH_ABSORIGIN_FOLLOW_NOROTATE = 2,
    PATTACH_CUSTOMORIGIN = 3,
    PATTACH_CUSTOMORIGIN_FOLLOW = 4,
    PATTACH_CUSTOMORIGIN_FOLLOW_NOROTATE = 5,
    PATTACH_POINT = 6,
    PATTACH_POINT_FOLLOW = 7,
    PATTACH_POINT_FOLLOW_NOROTATE = 8,
    PATTACH_EYES_FOLLOW = 9,
    PATTACH_OVERHEAD_FOLLOW = 10,
    PATTACH_WORLDORIGIN = 11,
    PATTACH_ROOTBONE_FOLLOW = 12,
    PATTACH_EYEANGLES_FOLLOW = 13,
    PATTACH_HEALTH = 14,
    PATTACH_FRIENDLINESS = 15,
    PATTACH_PLAYER_SUIT_POWER = 16,
    PATTACH_PLAYER_GRAPPLE_POWER = 17,
    PATTACH_PLAYER_SHARED_ENERGY = 18,
    PATTACH_WEAPON_CHARGE_FRACTION = 19,
    PATTACH_WEAPON_SMART_AMMO_LOCK_FRACTION = 20,
    PATTACH_WEAPON_READY_TO_FIRE_FRACTION = 21,
    PATTACH_WEAPON_RELOAD_FRACTION = 22,
    PATTACH_WEAPON_DRYFIRE_FRACTION = 23,
    PATTACH_WEAPON_CLIP_AMMO_FRACTION = 24,
    PATTACH_WEAPON_REMAINING_AMMO_FRACTION = 25,
    PATTACH_WEAPON_CLIP_AMMO_MAX = 26,
    PATTACH_WEAPON_STOCKPILE_AMMO_MAX = 27,
    PATTACH_WEAPON_LIFETIME_SHOTS = 28,
    PATTACH_WEAPON_AMMO_REGEN_RATE = 29,
    PATTACH_WEAPON_STOCKPILE_REGEN_FRAC = 30,
    PATTACH_BOOST_METER_FRACTION = 31,
    PATTACH_GLIDE_METER_FRACTION = 32,
    PATTACH_SHIELD_FRACTION = 33,
    PATTACH_STATUS_EFFECT_SEVERITY = 34,
    PATTACH_SCRIPT_NETWORK_VAR = 35,
    PATTACH_SCRIPT_NETWORK_VAR_GLOBAL = 36,
    PATTACH_SCRIPT_NETWORK_VAR_LOCAL_VIEW_PLAYER = 37,
    PATTACH_FRIENDLY_TEAM_SCORE = 38,
    PATTACH_FRIENDLY_TEAM_ROUND_SCORE = 39,
    PATTACH_ENEMY_TEAM_SCORE = 40,
    PATTACH_ENEMY_TEAM_ROUND_SCORE = 41,
    PATTACH_MINIMAP_SCALE = 42,
    PATTACH_SOUND_METER = 43,
    PATTACH_GAME_FULLY_INSTALLED_PROGRESS = 44,
    MAX_PATTACH_TYPES = 45,
};

int GetAttachTypeFromString(const char* pszString);

#define PARTICLE_DISPATCH_FROM_ENTITY (1 << 0)
#define PARTICLE_DISPATCH_RESET_PARTICLES (1 << 1)

// Manifest and dispatch entrypoints from the Source SDK. R2 loads packaged
// definitions through the particle system manager; these declarations do not
// imply that Source's PCF manifest loader is linked into Northstar.
void ParseParticleEffects(bool bLoadSheets, bool bPrecache);
void ParseParticleEffectsMap(const char* pMapName, bool bLoadSheets);
void GetParticleManifest(CUtlVector<CUtlString>& list);
void PrecacheStandardParticleSystems();
void ReloadParticleEffects();

void DispatchParticleEffect(const char* pszParticleName, ParticleAttachment_t iAttachType, C_BaseEntity* pEntity,
                            const char* pszAttachmentName, bool bResetAllParticlesOnEntity = false);
void DispatchParticleEffect(const char* pszParticleName, ParticleAttachment_t iAttachType, C_BaseEntity* pEntity = nullptr,
                            int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false);
void DispatchParticleEffect(const char* pszParticleName, Vector3D vecOrigin, QAngle vecAngles, C_BaseEntity* pEntity = nullptr);
void DispatchParticleEffect(const char* pszParticleName, Vector3D vecOrigin, Vector3D vecStart, QAngle vecAngles,
                            C_BaseEntity* pEntity = nullptr);
void DispatchParticleEffect(int iEffectIndex, Vector3D vecOrigin, Vector3D vecStart, QAngle vecAngles,
                            C_BaseEntity* pEntity = nullptr);
void StopParticleEffects(C_BaseEntity* pEntity);

#if defined(TF_CLIENT_DLL) || defined(TF_DLL)
struct te_tf_particle_effects_colors_t
{
    Vector3D m_vecColor1;
    Vector3D m_vecColor2;
};

struct te_tf_particle_effects_control_point_t
{
    ParticleAttachment_t m_eParticleAttachment;
    Vector3D m_vecOffset;
};

void DispatchParticleEffect(const char* pszParticleName, ParticleAttachment_t iAttachType, C_BaseEntity* pEntity,
                            const char* pszAttachmentName, Vector3D vecColor1, Vector3D vecColor2,
                            bool bUseColors = true, bool bResetAllParticlesOnEntity = false);
void DispatchParticleEffect(const char* pszParticleName, Vector3D vecOrigin, QAngle vecAngles, Vector3D vecColor1,
                            Vector3D vecColor2, bool bUseColors = true, C_BaseEntity* pEntity = nullptr,
                            int iAttachType = PATTACH_CUSTOMORIGIN);
#endif

static_assert(sizeof(ParticleAttachment_t) == 4);
static_assert(PATTACH_POINT_FOLLOW == 7 && PATTACH_WORLDORIGIN == 11 && MAX_PATTACH_TYPES == 45);
