#include "debugoverlay.h"

#include "Detour/Include/DetourNavMesh.h"
#include "client/cdll_client_int.h"
#include "dedicated/dedicated.h"
#include "mathlib/vector.h"
#include "mathlib/vplane.h"
#include "server/ai_navmesh.h"
#include "tier1/cvar.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>

DECLARE_MODULE(DebugOverlayHooks)

enum OverlayType_t
{
	OVERLAY_BOX = 0,
	OVERLAY_SPHERE,
	OVERLAY_LINE,
	OVERLAY_SMARTAMMO,
	OVERLAY_TRIANGLE,
	OVERLAY_SWEPT_BOX,
	// [Fifty]: the 2 bellow i did not confirm, rest are good
	OVERLAY_BOX2,
	OVERLAY_CAPSULE
};

struct OverlayBase_t
{
	OverlayBase_t()
	{
		m_Type = OVERLAY_BOX;
		m_nServerCount = -1;
		m_nCreationTick = -1;
		m_flEndTime = 0.0f;
		m_pNextOverlay = NULL;
	}

	OverlayType_t m_Type; // What type of overlay is it?
	int m_nCreationTick; // Duration -1 means go away after this frame #
	int m_nServerCount; // Latch server count, too
	float m_flEndTime; // When does this box go away
	OverlayBase_t* m_pNextOverlay;
	__int64 m_pUnk;
};

struct OverlayLine_t : public OverlayBase_t
{
	OverlayLine_t() { m_Type = OVERLAY_LINE; }

	Vector3D origin;
	Vector3D dest;
	int r;
	int g;
	int b;
	int a;
	bool noDepthTest;
};

struct OverlayBox_t : public OverlayBase_t
{
	OverlayBox_t() { m_Type = OVERLAY_BOX; }

	Vector3D origin;
	Vector3D mins;
	Vector3D maxs;
	QAngle angles;
	int r;
	int g;
	int b;
	int a;
};

struct OverlayTriangle_t : public OverlayBase_t
{
	OverlayTriangle_t() { m_Type = OVERLAY_TRIANGLE; }

	Vector3D p1;
	Vector3D p2;
	Vector3D p3;
	int r;
	int g;
	int b;
	int a;
	bool noDepthTest;
};

struct OverlaySweptBox_t : public OverlayBase_t
{
	OverlaySweptBox_t() { m_Type = OVERLAY_SWEPT_BOX; }

	Vector3D start;
	Vector3D end;
	Vector3D mins;
	Vector3D maxs;
	QAngle angles;
	int r;
	int g;
	int b;
	int a;
};

struct OverlaySphere_t : public OverlayBase_t
{
	OverlaySphere_t() { m_Type = OVERLAY_SPHERE; }

	Vector3D vOrigin;
	float flRadius;
	int nTheta;
	int nPhi;
	int r;
	int g;
	int b;
	int a;
	bool m_bWireframe;
};

static bool (*OverlayBase_t__IsDead)(OverlayBase_t* a1);
static void (*OverlayBase_t__DestroyOverlay)(OverlayBase_t* a1);

static ConVar* Cvar_enable_debug_overlays;
static ConVar* Cvar_navmesh_debug_hull;
static ConVar* Cvar_navmesh_debug_camera_radius;
static ConVar* Cvar_navmesh_debug_lossy_optimization;

LPCRITICAL_SECTION s_OverlayMutex;

OverlayBase_t** s_pOverlays;

int* g_nRenderTickCount;
int* g_nOverlayTickCount;

static uint64_t PackNavmeshOutline(const Vector3D& v1, const Vector3D& v2)
{
    int16_t x1 = static_cast<int16_t>(v1.x);
    int16_t x2 = static_cast<int16_t>(v2.x);
    int16_t y1 = static_cast<int16_t>(v1.y);
    int16_t y2 = static_cast<int16_t>(v2.y);
    if (x1 < x2)
        std::swap(x1, x2);
    if (y1 < y2)
        std::swap(y1, y2);

    return (static_cast<uint64_t>(static_cast<uint16_t>(x1)) << 48) | (static_cast<uint64_t>(static_cast<uint16_t>(x2)) << 32) |
           (static_cast<uint64_t>(static_cast<uint16_t>(y1)) << 16) | static_cast<uint16_t>(y2);
}

static void DrawNavmeshPolys()
{
    if (!Cvar_navmesh_debug_hull || !Cvar_navmesh_debug_camera_radius || !Cvar_navmesh_debug_lossy_optimization || !g_pClientTools || !RenderLine ||
        !RenderTriangle)
        return;

    const int nHull = Cvar_navmesh_debug_hull->GetInt();
    if (nHull < 1 || nHull > 4)
        return;

    const float fCamRadius = Cvar_navmesh_debug_camera_radius->GetFloat();
    const float fCamRadiusSquared = fCamRadius * fCamRadius;
    if (!std::isfinite(fCamRadius) || fCamRadius <= 0.0f || !std::isfinite(fCamRadiusSquared))
        return;

    const dtNavMesh* pNavMesh = GetNavMeshForHull(nHull);
    if (!pNavMesh)
        return;

    Vector3D vCamera;
    QAngle aCamera;
    float fFov;
    if (!g_pClientTools->GetLocalPlayerEyePosition(vCamera, aCamera, fFov) || !vCamera.IsValid())
        return;

    VPlane CullPlane;
    CullPlane.Init(vCamera - aCamera.GetNormal() * 256.0f, aCamera);
    const bool bOptimize = Cvar_navmesh_debug_lossy_optimization->GetBool();

	std::optional<std::unordered_set<uint64_t>> Outlines;

    const int nMaxTiles = pNavMesh->getMaxTiles();
    for (int i = 0; i < nMaxTiles; ++i)
    {
        const dtMeshTile* pTile = pNavMesh->getTile(i);
        if (!pTile || !pTile->header)
            continue;

        for (int j = 0; j < pTile->header->polyCount; ++j)
        {
            const dtPoly* pPoly = &pTile->polys[j];
            const Vector3D vCenter(pPoly->center.x, pPoly->center.y, pPoly->center.z);
            const Vector3D vDelta = vCenter - vCamera;
            if (!(vDelta.Dot(vDelta) <= fCamRadiusSquared) || CullPlane.GetPointSide(vCenter) != SIDE_FRONT)
                continue;

            if (pPoly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
            {
                const dtOffMeshConnection* pCon = &pTile->offMeshCons[j - pTile->header->offMeshBase];
                RenderLine(Vector3D(pCon->posa.x, pCon->posa.y, pCon->posa.z), Vector3D(pCon->posb.x, pCon->posb.y, pCon->posb.z),
                           Color(255, 250, 50, 255), true);
                continue;
            }

            const dtPolyDetail* pDetail = &pTile->detailMeshes[j];
            for (int k = 0; k < pDetail->triCount; ++k)
            {
                const unsigned char* pTriangle = &pTile->detailTris[(pDetail->triBase + k) * 4];
                Vector3D v[3];
                for (int l = 0; l < 3; ++l)
                {
                    const rdVec3D& Vertex = pTriangle[l] < pPoly->vertCount ? pTile->verts[pPoly->verts[pTriangle[l]]]
                                                                            : pTile->detailVerts[pDetail->vertBase + pTriangle[l] - pPoly->vertCount];
                    v[l] = Vector3D(Vertex.x, Vertex.y, Vertex.z);
                }

                RenderTriangle(v[0], v[1], v[2], Color(110, 200, 220, 160), true);
                if (bOptimize && !Outlines)
                    Outlines.emplace();

                for (int l = 0; l < 3; ++l)
                {
                    const Vector3D& vStart = v[l];
                    const Vector3D& vEnd = v[(l + 1) % 3];
                    if (!bOptimize || Outlines->insert(PackNavmeshOutline(vStart, vEnd)).second)
                        RenderLine(vStart, vEnd, Color(0, 0, 150), true);
                }
            }
        }
    }
}

static void h_DrawOverlay(OverlayBase_t* pOverlay)
{
	EnterCriticalSection(s_OverlayMutex);

	switch (pOverlay->m_Type)
	{
	case OVERLAY_SMARTAMMO:
	case OVERLAY_LINE:
	{
		OverlayLine_t* pLine = static_cast<OverlayLine_t*>(pOverlay);
		RenderLine(pLine->origin, pLine->dest, Color(pLine->r, pLine->g, pLine->b, pLine->a), pLine->noDepthTest);
	}
	break;
	case OVERLAY_BOX:
	{
		OverlayBox_t* pCurrBox = static_cast<OverlayBox_t*>(pOverlay);
		if (pCurrBox->a > 0)
		{
			RenderBox(
				pCurrBox->origin,
				pCurrBox->angles,
				pCurrBox->mins,
				pCurrBox->maxs,
				Color(pCurrBox->r, pCurrBox->g, pCurrBox->b, pCurrBox->a),
				false,
				false);
		}
		if (pCurrBox->a < 255)
		{
			RenderWireframeBox(
				pCurrBox->origin,
				pCurrBox->angles,
				pCurrBox->mins,
				pCurrBox->maxs,
				Color(pCurrBox->r, pCurrBox->g, pCurrBox->b, 255),
				false,
				false);
		}
	}
	break;
	case OVERLAY_TRIANGLE:
	{
		OverlayTriangle_t* pTriangle = static_cast<OverlayTriangle_t*>(pOverlay);
		RenderTriangle(
			pTriangle->p1,
			pTriangle->p2,
			pTriangle->p3,
			Color(pTriangle->r, pTriangle->g, pTriangle->b, pTriangle->a),
			pTriangle->noDepthTest);
	}
	break;
	case OVERLAY_SWEPT_BOX:
	{
		OverlaySweptBox_t* pBox = static_cast<OverlaySweptBox_t*>(pOverlay);
		RenderWireframeSweptBox(
			pBox->start, pBox->end, pBox->angles, pBox->mins, pBox->maxs, Color(pBox->r, pBox->g, pBox->b, pBox->a), false);
	}
	break;
	case OVERLAY_SPHERE:
	{
		OverlaySphere_t* pSphere = static_cast<OverlaySphere_t*>(pOverlay);
		RenderSphere(
			pSphere->vOrigin,
			pSphere->flRadius,
			pSphere->nTheta,
			pSphere->nPhi,
			Color(pSphere->r, pSphere->g, pSphere->b, pSphere->a),
			false);
	}
	break;
	default:
	{
		spdlog::warn("Unimplemented overlay type {}", static_cast<int>(pOverlay->m_Type));
	}
	break;
	}

	LeaveCriticalSection(s_OverlayMutex);
}

DECLARE_HOOK_FN(DrawOverlay, engine.dll + 0xABCB0, h_DrawOverlay)

DECLARE_HOOK(DrawAllOverlays, engine.dll + 0xAB780, [](auto& hook, bool bRender)
{
	NOTE_UNUSED(hook);
	EnterCriticalSection(s_OverlayMutex);

	OverlayBase_t* pCurrOverlay = *s_pOverlays; // rbx
	OverlayBase_t* pPrevOverlay = nullptr; // rsi
	OverlayBase_t* pNextOverlay = nullptr; // rdi

	int m_nCreationTick; // eax
	bool bShouldDraw; // zf
	int m_pUnk; // eax

	while (pCurrOverlay)
	{
		if (OverlayBase_t__IsDead(pCurrOverlay))
		{
			if (pPrevOverlay)
			{
				pPrevOverlay->m_pNextOverlay = pCurrOverlay->m_pNextOverlay;
			}
			else
			{
				*s_pOverlays = pCurrOverlay->m_pNextOverlay;
			}

			pNextOverlay = pCurrOverlay->m_pNextOverlay;
			OverlayBase_t__DestroyOverlay(pCurrOverlay);
			pCurrOverlay = pNextOverlay;
		}
		else
		{
			if (pCurrOverlay->m_nCreationTick == -1)
			{
				m_pUnk = pCurrOverlay->m_pUnk;

				if (m_pUnk == -1)
				{
					bShouldDraw = true;
				}
				else
				{
					bShouldDraw = m_pUnk == *g_nOverlayTickCount;
				}
			}
			else
			{
				bShouldDraw = pCurrOverlay->m_nCreationTick == *g_nRenderTickCount;
			}

			if (bShouldDraw && bRender && (Cvar_enable_debug_overlays->GetBool() || pCurrOverlay->m_Type == OVERLAY_SMARTAMMO))
			{
				h_DrawOverlay(pCurrOverlay);
			}

			pPrevOverlay = pCurrOverlay;
			pCurrOverlay = pCurrOverlay->m_pNextOverlay;
		}
    }

    if (bRender && Cvar_enable_debug_overlays->GetBool())
        DrawNavmeshPolys();

    LeaveCriticalSection(s_OverlayMutex);
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", DebugOverlay, ConVar, [](CModule module)
{
	DISPATCH_MODULE(DebugOverlayHooks)

	OverlayBase_t__IsDead = module.Offset(0xACAC0).RCast<decltype(OverlayBase_t__IsDead)>();
	OverlayBase_t__DestroyOverlay = module.Offset(0xAB680).RCast<decltype(OverlayBase_t__DestroyOverlay)>();

	RenderLine = module.Offset(0x192A70).RCast<decltype(RenderLine)>();
	RenderBox = module.Offset(0x192520).RCast<decltype(RenderBox)>();
	RenderWireframeBox = module.Offset(0x193DA0).RCast<decltype(RenderWireframeBox)>();
	RenderWireframeSweptBox = module.Offset(0x1945A0).RCast<decltype(RenderWireframeSweptBox)>();
	RenderTriangle = module.Offset(0x193940).RCast<decltype(RenderTriangle)>();
	RenderAxis = module.Offset(0x1924D0).RCast<decltype(RenderAxis)>();
	RenderSphere = module.Offset(0x194170).RCast<decltype(RenderSphere)>();
	RenderUnknown = module.Offset(0x1924E0).RCast<decltype(RenderUnknown)>();

	s_OverlayMutex = module.Offset(0x10DB0A38).RCast<LPCRITICAL_SECTION>();

	s_pOverlays = module.Offset(0x10DB0968).RCast<OverlayBase_t**>();

	g_nRenderTickCount = module.Offset(0x10DB0984).RCast<int*>();
	g_nOverlayTickCount = module.Offset(0x10DB0980).RCast<int*>();

	// not in g_pCVar->FindVar by this point for whatever reason, so have to get from memory
	Cvar_enable_debug_overlays = module.Offset(0x10DB0990).RCast<ConVar*>();
	Cvar_enable_debug_overlays->SetValue(false);
	Cvar_enable_debug_overlays->m_pszDefaultValue = (char*)"0";
	Cvar_enable_debug_overlays->AddFlags(FCVAR_CHEAT);

    Cvar_navmesh_debug_hull =
        new ConVar("navmesh_debug_hull", "0", FCVAR_RELEASE, "0 = off, 1 = small/Human, 2 = med_short/Prowler, 3 = medium/Reaper, 4 = large/Titan");
    Cvar_navmesh_debug_camera_radius = new ConVar("navmesh_debug_camera_radius", "1000", FCVAR_RELEASE, "Radius in which to draw navmeshes");
    Cvar_navmesh_debug_lossy_optimization =
        new ConVar("navmesh_debug_lossy_optimization", "1", FCVAR_RELEASE, "Whether to enable lossy navmesh debug draw optimizations");
})
