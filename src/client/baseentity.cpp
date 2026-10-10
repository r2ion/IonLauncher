#include "client/baseentity.h"
#include "client/baseanimating.h"
#include "client/baseanimatingoverlay.h"
#include "client/cdll_client_int.h"
#include "client/player.h"
#include "client/prediction.h"
#include "client/weaponx.h"
#include "engine/cdll_int.h"
#include "engine/client/clientstate.h"
#include "engine/r2engine.h"
#include "game/shared/predictioncopy.h"
#include "tier0/callbacks.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "util/utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

DECLARE_MODULE(ClientBaseEntity)

C_BaseEntity_ValidateScriptScope_t C_BaseEntity__ValidateScriptScope;
C_BaseEntity_InvalidatePhysicsRecursive_t C_BaseEntity__InvalidatePhysicsRecursive;
C_BaseEntity_ProcessInterpolatedList_t C_BaseEntity__ProcessInterpolatedList;
C_BaseEntity_SetupForInterpolation_t C_BaseEntity__SetupForInterpolation;
C_BaseEntity_FinishInterpolation_t C_BaseEntity__FinishInterpolation;
C_BaseEntity_CheckInterpolatedTransformChanges_t C_BaseEntity__CheckInterpolatedTransformChanges;
C_BaseEntity_GetPredictedFrame_t C_BaseEntity__GetPredictedFrame;
C_BaseEntityIterator_Constructor_t C_BaseEntityIterator__C_BaseEntityIterator;
C_BaseEntityIterator_Next_t C_BaseEntityIterator__Next;
GetPredictables_t g_pfnGetPredictables;

bool* C_BaseEntity__s_bInterpolate;
ConVar** cl_interpolate;
ConVar** g_pInterpolateOnParentChange;

float C_BaseEntity::GetInterpolationFraction(float currentTime, float secondSnapshotTime, const SingleSnapshotValues* secondSnapshot) const
{
    const auto& clock = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState.interpolationClock;
    const float startTime = g_pClientGlobals->currentSnapTime;
    float elapsed = currentTime - startTime;
    if (!GetPredictable() && secondSnapshot == m_lerpData.futureSnap && clock.active && currentTime == clock.currentTime &&
        startTime == clock.snapshotStartTime && secondSnapshotTime == clock.snapshotEndTime)
        elapsed = static_cast<float>((static_cast<double>(currentTime) - static_cast<double>(startTime)) + clock.remainder);
    const float fraction = elapsed / (secondSnapshotTime - startTime);
    const float nonnegative = fraction > 0.0f ? fraction : 0.0f;
    return nonnegative < 1.0f ? nonnegative : 1.0f;
}

bool C_BaseEntity::IsInterpolationEnabled()
{
    return *C_BaseEntity__s_bInterpolate;
}

void C_BaseEntity::CapturePredictedRenderSample(PredictedRenderSample_t& sample)
{
    sample.parent = m_pMoveParent;
    sample.modelIndex = m_currentFrame.modelIndex;
    sample.origin = m_localOrigin;
    sample.angles = m_localAngles;
    sample.viewOffset = m_currentFrame.viewOffset;
    sample.cycle = m_currentFrame.animCycle;
    if (const C_BaseAnimating* const animating = GetBaseAnimating())
    {
        sample.animation = animating->m_currentFrameBaseAnimating;
        sample.ikOffset = animating->m_flEstIkOffset;
    }
    if (const C_BaseAnimatingOverlay* const overlay = GetBaseAnimatingOverlay())
        sample.overlays = overlay->m_currentFrameAnimationOverlay;
    if (const C_Player* const player = MyPlayerPointer())
    {
        sample.player.timeBase = player->m_currentFramePlayer.timeBase;
        sample.player.m_flHullHeight = player->m_currentFramePlayer.m_flHullHeight;
        sample.player.m_traversalAnimProgress = player->m_currentFramePlayer.m_traversalAnimProgress;
        sample.player.m_sprintTiltFrac = player->m_currentFramePlayer.m_sprintTiltFrac;
        sample.viewConeParity = player->m_viewConeParity;
        std::memcpy(&sample.localPlayer, &player->m_currentFrameLocalPlayer.m_viewConeAngleMin, sizeof(LocalPlayerData));
    }
    if (const C_WeaponX* const weapon = MyWeaponXPointer())
        std::memcpy(sample.smartAmmoFractions, weapon->m_smartAmmo.currentFrameSmartAmmoFractions, sizeof(sample.smartAmmoFractions));
}

C_BaseEntityIterator::C_BaseEntityIterator()
{
    C_BaseEntityIterator__C_BaseEntityIterator(this);
}

C_BaseEntity* C_BaseEntityIterator::Next()
{
    return C_BaseEntityIterator__Next(this);
}

CPredictableList* GetPredictables(int splitScreenSlot)
{
    return g_pfnGetPredictables(splitScreenSlot);
}

bool C_BaseEntity::ValidateScriptScope()
{
    return C_BaseEntity__ValidateScriptScope(this);
}

HSCRIPT C_BaseEntity::GetScriptInstance()
{
    if (!ValidateScriptScope())
        return INVALID_HSCRIPT;

    return m_ScriptScope.m_hScope;
}

void C_BaseEntity::InvalidatePhysicsRecursive(unsigned int changeFlags)
{
    C_BaseEntity__InvalidatePhysicsRecursive(this, changeFlags);
}

void C_BaseEntity::ProcessInterpolatedList()
{
    C_BaseEntity__ProcessInterpolatedList();
}

int C_BaseEntity::SetupForInterpolation()
{
    return C_BaseEntity__SetupForInterpolation(this);
}

void C_BaseEntity::FinishInterpolation()
{
    C_BaseEntity__FinishInterpolation(this);
}

void C_BaseEntity::CheckInterpolatedTransformChanges(const Vector3D& previousOrigin, const QAngle& previousAngles, unsigned int changeFlags)
{
    C_BaseEntity__CheckInterpolatedTransformChanges(this, &previousOrigin, &previousAngles, changeFlags);
}

const PredictedEntityState* C_BaseEntity::GetPredictedFrame(int commandNumber) const
{
    return C_BaseEntity__GetPredictedFrame(this, commandNumber);
}

void C_BaseEntity::InterpolateServerEntities()
{
    CGlobalVarsBase* const globals = g_pClientGlobals;
    const CClientState* const client = GetBaseLocalClient();
    auto& render = client->GetClientStateExtended()->m_RenderState;
    auto& clock = render.interpolationClock;
    clock.active = false;

	if (client && globals->curtime == client->m_clientTime && !client->m_bClockCorrectionEnabled && !g_pEngineClient->IsPlayingDemo() &&
        std::isfinite(globals->curtime) && std::isfinite(globals->currentSnapTime) && std::isfinite(globals->futureSnapTime) &&
        globals->futureSnapTime > globals->currentSnapTime)
    {
        clock.remainder = client->GetPreciseClientTime() - static_cast<double>(client->m_clientTime);
        clock.currentTime = globals->curtime;
        clock.snapshotStartTime = globals->currentSnapTime;
        clock.snapshotEndTime = globals->futureSnapTime;
        clock.active = clock.remainder != 0.0;
    }

    const bool wasEnabled = *C_BaseEntity__s_bInterpolate;
    globals->snapLerp = 0.0f;
    if (globals->futureSnapTime > globals->currentSnapTime)
    {
        float elapsed = globals->curtime - globals->currentSnapTime;
        if (clock.active)
            elapsed = static_cast<float>((static_cast<double>(globals->curtime) - static_cast<double>(globals->currentSnapTime)) + clock.remainder);
        const float fraction = elapsed / (globals->futureSnapTime - globals->currentSnapTime);
        const float nonnegative = fraction > 0.0f ? fraction : 0.0f;
        globals->snapLerp = nonnegative < 1.0f ? nonnegative : 1.0f;
    }
    *C_BaseEntity__s_bInterpolate = (*cl_interpolate)->GetBool();
    if (g_pEngineClient->IsPlayingTimeDemo())
        *C_BaseEntity__s_bInterpolate = false;
    if (wasEnabled && !*C_BaseEntity__s_bInterpolate)
    {
        C_BaseEntityIterator iterator;
        while (C_BaseEntity* const entity = iterator.Next())
            entity->ResetIK();
    }
    C_BaseEntity::ProcessInterpolatedList();
    globals->lastInterpolationTime = globals->curtime;
    clock.active = false;
}

int C_BaseEntity::InterpolateFieldsInternal(float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime)
{
    const Vector3D previousOrigin = m_localOrigin;
    const QAngle previousAngles = m_localAngles;
    const int status = SetupForInterpolation();
    if (status && (m_shouldInterpolateAngles || m_shouldInterpolateOrigin))
    {
        const auto& first = *m_lerpData.currentSnap;
        const auto& second = *secondSnapshot;
        const float fraction = GetInterpolationFraction(currentTime, secondSnapshotTime, secondSnapshot);
        const bool changedParent = first.moveParent.ToInt() != second.moveParent.ToInt() && !(*g_pInterpolateOnParentChange)->GetBool();
        m_localOrigin = m_shouldInterpolateOrigin && !changedParent ? (second.origin - first.origin) * fraction + first.origin : first.origin;

        if (m_shouldInterpolateAngles && !changedParent && first.angles != second.angles)
        {
            alignas(16) Quaternion firstRotation, secondRotation, rotation;
            AngleQuaternion(first.angles, firstRotation);
            AngleQuaternion(second.angles, secondRotation);
            QuaternionSlerp(firstRotation, secondRotation, fraction, rotation);
            QuaternionAngles(rotation, m_localAngles);
        }
        else
            m_localAngles = first.angles;
        CheckInterpolatedTransformChanges(previousOrigin, previousAngles, 0);
    }
    FinishInterpolation();
    return status;
}

void C_BaseEntity::RehydrateMovementNormals(int commandNumber)
{
    const auto* frame = GetPredictedFrame(commandNumber);
    if (!frame || !frame->active || !frame->serializedData)
        return;
    const auto* map = reinterpret_cast<const prediction_datamap_t*>(GetPredDescMap());
    if (!map->optimizedMap)
        return;

    auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    auto& offsets = render.movementNormalFieldOffsets;
    if (render.movementNormalPredictionMap != map->optimizedMap)
    {
        for (auto& fieldOffsets : offsets)
            fieldOffsets = {};
        const auto& fields = map->optimizedMap->m_Info[1].m_Flat;
        for (int i = 0; i < fields.Count(); ++i)
        {
            const auto& field = fields[i];
            if (field.fieldType != FIELD_VECTOR || field.fieldSize != 1)
                continue;
            const std::string_view name(field.fieldName);
            const int index = name == "m_upDir" ? 0 : (name == "m_upDirPredicted" ? 1 : -1);
            if (index >= 0)
                offsets[index] = {field.flatOffset[TD_OFFSET_NORMAL], field.flatOffset[TD_OFFSET_PACKED]};
        }
        render.movementNormalPredictionMap = map->optimizedMap;
    }

    for (const auto& fieldOffsets : offsets)
    {
        if (fieldOffsets.normal < 0 || fieldOffsets.packed < 0)
            continue;
        auto& received = *reinterpret_cast<Vector3D*>(reinterpret_cast<std::byte*>(this) + fieldOffsets.normal);
        const auto& predicted = *reinterpret_cast<const Vector3D*>(frame->serializedData + fieldOffsets.packed);
        if (std::memcmp(&predicted, &received, sizeof(received)) == 0 || !predicted.IsValid())
            continue;

        const float x = std::abs(predicted.x);
        const float y = std::abs(predicted.y);
        const float z = std::abs(predicted.z);
        const int axis = x > y && x > z ? 0 : (y > z ? 1 : 2);
        float component[2];
        int next = 0;
        for (int i = 0; i < 3; ++i)
        {
            if (i == axis)
                continue;

            const float scaled = static_cast<float>((static_cast<double>(predicted[i]) + 0.7071068) * 722.6631138400651);
            const int value = std::clamp(static_cast<int>(std::floor(scaled + 0.5f)), 0, 1023);
            component[next++] = static_cast<float>((value - 511) * 0.00138377063934384);
        }
        Vector3D encoded;
        encoded[axis] = std::sqrt(1.0f - (component[0] * component[0] + component[1] * component[1]));
        if (predicted[axis] < 0.0f)
            encoded[axis] = -encoded[axis];
        next = 0;
        for (int i = 0; i < 3; ++i)
            if (i != axis)
                encoded[i] = component[next++];
        if (std::memcmp(&encoded, &received, sizeof(received)) == 0)
            received = predicted;
    }
}

DECLARE_HOOK(C_BaseEntity_PostNetworkDataReceived, client.dll + 0x3EDAB0,
             [](auto& hook, C_BaseEntity* self, int commandNumber, int commandsAcknowledged) -> bool
{
    const int previousCommand = g_nPredictionErrorCommand;
    const ScopeGuard restoreCommand([previousCommand] { g_nPredictionErrorCommand = previousCommand; });
    g_nPredictionErrorCommand = commandNumber;
    if (commandsAcknowledged > 0 && self->GetPredictable() && self == C_Player::GetLocalPlayer())
        self->RehydrateMovementNormals(commandNumber);
    return hook.Original(self, commandNumber, commandsAcknowledged);
})

DECLARE_HOOK(C_BaseEntity_InterpolateServerEntities, client.dll + 0x3EB550, [](auto&) { C_BaseEntity::InterpolateServerEntities(); })

DECLARE_HOOK(C_BaseEntity_InterpolateFieldsInternal, client.dll + 0x3EB250,
             [](auto&, C_BaseEntity* self, float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime) -> int
{ return self->C_BaseEntity::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime); })

DECLARE_HOOK(C_BaseEntity_ReleaseRenderState, client.dll + 0x3EE960, [](auto& hook, IClientNetworkable* self)
{
    C_BaseEntity* const entity = self->GetIClientUnknown()->GetBaseEntity();
    g_pClientSidePrediction->RestoreRenderState(entity);
    if (C_BaseAnimating* const animating = entity->GetBaseAnimating())
        animating->RestoreRemoteSequencePresentation();
    hook.Original(self);
})

ON_DLL_LOAD_CLIENT("client.dll", ClientBaseEntityMethods, [](CModule module)
{
    C_BaseEntity__ValidateScriptScope = module.Offset(0xC2FB0).RCast<C_BaseEntity_ValidateScriptScope_t>();
    C_BaseEntity__InvalidatePhysicsRecursive = module.Offset(0xC3150).RCast<C_BaseEntity_InvalidatePhysicsRecursive_t>();
    C_BaseEntity__ProcessInterpolatedList = module.Offset(0x3EE0A0).RCast<C_BaseEntity_ProcessInterpolatedList_t>();
    C_BaseEntity__SetupForInterpolation = module.Offset(0x3DB0C0).RCast<C_BaseEntity_SetupForInterpolation_t>();
    C_BaseEntity__FinishInterpolation = module.Offset(0x3F0920).RCast<C_BaseEntity_FinishInterpolation_t>();
    C_BaseEntity__CheckInterpolatedTransformChanges = module.Offset(0x3E1620).RCast<C_BaseEntity_CheckInterpolatedTransformChanges_t>();
    C_BaseEntity__GetPredictedFrame = module.Offset(0x3E4250).RCast<C_BaseEntity_GetPredictedFrame_t>();
    C_BaseEntity__s_bInterpolate = module.Offset(0xB33CE3).RCast<bool*>();
    cl_interpolate = module.Offset(0x26CF838).RCast<ConVar**>();
    g_pInterpolateOnParentChange = module.Offset(0x23FA108).RCast<ConVar**>();
    C_BaseEntityIterator__C_BaseEntityIterator = module.Offset(0x1A5ED0).RCast<C_BaseEntityIterator_Constructor_t>();
    C_BaseEntityIterator__Next = module.Offset(0x1A6860).RCast<C_BaseEntityIterator_Next_t>();
    g_pfnGetPredictables = module.Offset(0x3E4230).RCast<GetPredictables_t>();
    DISPATCH_MODULE(ClientBaseEntity)
})
