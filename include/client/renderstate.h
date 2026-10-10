#pragma once

#include "client/baseanimating.h"
#include "engine/shared/signonstate.h"

#include <vector>

class C_BaseViewModel;
class CClientState;
class CNetChan;
struct optimized_datamap_t;

struct RenderInterpolationClock_t
{
    double remainder = 0.0;
    float currentTime = 0.0f;
    float snapshotStartTime = 0.0f;
    float snapshotEndTime = 0.0f;
    bool active = false;
};

struct MovementNormalFieldOffsets_t
{
    int normal = -1;
    int packed = -1;
};

struct PredictedRenderSample_t
{
    int commandNumber = -1;
    float frameTime = 0.0f;
    CBaseHandle parent;
    int modelIndex;
    Vector3D origin;
    QAngle angles;
    Vector3D viewOffset;
    float cycle;
    float ikOffset;
    AnimatingData animation;
    AnimationOverlayData overlays;
    PlayerData player;
    LocalPlayerData localPlayer;
    int viewConeParity;
    float smartAmmoFractions[8];
};

struct PredictedRenderEntity_t
{
    CHandle<C_BaseEntity> handle;
    PredictedRenderSample_t samples[2];
    PredictedRenderSample_t saved;
    int latest = 0;
    unsigned int changed = 0;
    unsigned int changedLayers = 0;
};

struct RemoteSequencePresentation_t
{
    CHandle<C_BaseAnimating> handle;
    CBaseHandle parent;
    const studiohdr_t* model = nullptr;
    int serverCount = -1;
    int frame = -1;
    bool applied = false;
    C_SequenceTransitioner rendered;
    C_SequenceTransitioner saved;
};

struct ZoomPresentation_t
{
    CHandle<C_Player> player;
    CHandle<C_WeaponX> weapon;
    int serverCount = -1;
    int frame = -1;
    double fraction = 0.0;
    bool active = false;
};

struct CrosshairPresentation_t
{
    CHandle<C_Player> player;
    Vector3D direction;
    int frame = -1;
    bool active = false;
};

struct ViewModelPresentation_t
{
    CHandle<C_BaseViewModel> entity;
    float cycle;
    float playbackRate;
    float overlayCycles[8];
    bool active = false;
};

struct PreciseClientTimeState_t
{
    const CClientState* owner = nullptr;
    const CNetChan* channel = nullptr;
    int serverCount = 0;
    eSignonState signonState{};
    double remainder = 0.0;
    float clientTime = 0.0f;
};

struct ClientRenderState_t
{
    RenderInterpolationClock_t interpolationClock;
    const optimized_datamap_t* movementNormalPredictionMap = nullptr;
    MovementNormalFieldOffsets_t movementNormalFieldOffsets[2];
    std::vector<PredictedRenderEntity_t> predictedEntities;
    CHandle<C_Player> predictedPlayer;
    int predictedServerCount = 0;
    bool cameraTimeActive = false;
    float cameraTime = 0.0f;
    std::vector<RemoteSequencePresentation_t> remoteSequences;
    bool remoteSequencesActive = false;
    ZoomPresentation_t zoom;
    CrosshairPresentation_t crosshair;
    ViewModelPresentation_t viewModel;
    bool inRenderStart = false;
};
