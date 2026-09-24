#include "server/ai_navmesh.h"
#include "core/convar/concommand.h"
#include "core/filesystem/filesystem.h"
#include "core/tier0.h"
#include "engine/hoststate.h"
#include "engine/r2engine.h"
#include "modsystem/modmanager.h"
#include "navmesh/navmesh_bsp.h"
#include "navmesh/navmesh_builder.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>

static dtNavMesh** s_ppNavMeshes = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Borrow a loaded local-server graph for the current debug draw.
//-----------------------------------------------------------------------------
const dtNavMesh* GetNavMeshForHull(int nHull)
{
    if (!s_ppNavMeshes || nHull < 1 || nHull > 4 || !g_pServerState || (*g_pServerState != ss_active && *g_pServerState != ss_paused) ||
        !g_pHostState || g_pHostState->m_iCurrentState != HostState_t::HS_RUN || g_pHostState->m_iNextState != HostState_t::HS_RUN)
        return nullptr;

    return s_ppNavMeshes[nHull - 1];
}

ON_DLL_LOAD("server.dll", ServerAiNavMesh, [](CModule Module) { s_ppNavMeshes = Module.Offset(0x105F5D0).RCast<dtNavMesh**>(); })
//-----------------------------------------------------------------------------
// Purpose: Generate the loaded map's navmeshes through the SDK builder.
//          Live AI graphs remain unchanged until the next map load.
//-----------------------------------------------------------------------------
static void NavMesh_Generate_f(const CCommand& args)
{
    if (args.ArgC() > 2)
    {
        spdlog::info("Usage: navmesh_generate [small|med_short|medium|large|all]");
        return;
    }
    if (!ThreadInMainThread() || !g_pFilesystem || !g_pServerState || (*g_pServerState != ss_active && *g_pServerState != ss_paused) ||
        !g_pHostState || g_pHostState->m_iCurrentState != HostState_t::HS_RUN || g_pHostState->m_iNextState != HostState_t::HS_RUN || !g_pGlobals ||
        !g_pGlobals->m_pMapName || !*g_pGlobals->m_pMapName)
    {
        spdlog::warn("[navmesh] Load a local listen-server or dedicated-server map before generating navmeshes.");
        return;
    }

    try
    {
        const std::string MapName = g_pGlobals->m_pMapName;
        for (const char Character : MapName)
        {
            if (!((Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') || (Character >= '0' && Character <= '9') ||
                  Character == '_' || Character == '-'))
                throw std::invalid_argument("invalid map name");
        }

        std::optional<HullType_t> RequestedHull;
        const char* pHullName = args.ArgC() == 2 ? args.Arg(1) : "all";
        if (std::string_view(pHullName) != "all")
        {
            RequestedHull = CNavMeshBuilder::ParseHullType(pHullName);
            if (!RequestedHull || *RequestedHull == HullType_t::ExtraLarge)
                throw std::invalid_argument("hull must be small, med_short, medium, large or all");
        }
        const std::filesystem::path OutputDirectory = GetGeneratedAssetsPath() / "maps" / "navmesh";
        std::filesystem::create_directories(OutputDirectory);

        const double flStartTime = g_PlatFloatTime();
        spdlog::info("[navmesh] Generating {} navmeshes for {} (generation blocks the local server)", pHullName, MapName);
        BuildSettings_t Settings;
        // Source collision meshes contain both windings; winding is not walkability.
        Settings.IgnoreWinding = true;
        int nWrittenCount = 0;
        for (const bool bTitanCollision : {false, true})
        {
            if (RequestedHull && bTitanCollision != (*RequestedHull == HullType_t::Large))
                continue;

            CNavMeshMap Map(MapName.c_str(), bTitanCollision, ReadVPKFile);

            CGeneratedNavMesh SmallBaseline;
            CNavMeshGeometry FloorRepair;
            if (!bTitanCollision && (!RequestedHull || *RequestedHull == HullType_t::Small || *RequestedHull == HullType_t::MediumShort))
            {
                const HullSettings_t& SmallHull = CNavMeshBuilder::GetHullSettings(HullType_t::Small);
                Map.AddAuthoredTraversal(SmallHull);
                SmallBaseline = CNavMeshBuilder(Map.m_Geometry, SmallHull, Settings).Build();
                FloorRepair = Map.CreateRenderFloorRepair(*SmallBaseline.Get());
            }
            const bool bHasFloorRepair = !FloorRepair.m_Triangles.empty();

            // Build medium before appending infantry-only floor geometry.
            for (const HullType_t Type : {HullType_t::Medium, HullType_t::Small, HullType_t::MediumShort, HullType_t::Large})
            {
                if ((RequestedHull && *RequestedHull != Type) || bTitanCollision != (Type == HullType_t::Large))
                    continue;

                const HullSettings_t& Hull = CNavMeshBuilder::GetHullSettings(Type);
                if ((Type == HullType_t::Small || Type == HullType_t::MediumShort) && !FloorRepair.m_Triangles.empty())
                {
                    Map.m_Geometry.Append(std::move(FloorRepair));
                }
                Map.m_Geometry.m_OffMeshConnections.clear();
                Map.m_Geometry.m_AreaVolumes.clear();
                Map.AddAuthoredTraversal(Hull);
                CGeneratedNavMesh Mesh = Type == HullType_t::Small && !bHasFloorRepair ? std::move(SmallBaseline)
                                                                                       : CNavMeshBuilder(Map.m_Geometry, Hull, Settings).Build();
                const std::filesystem::path OutputPath = OutputDirectory / (MapName + "_" + Hull.Suffix + ".nm");
                const std::u8string OutputName = OutputPath.u8string();
                const NavMeshSummary_t Summary = Mesh.Save(OutputPath);
                ++nWrittenCount;
                const BuildStatistics_t& Statistics = Mesh.Statistics();
                spdlog::info("[navmesh] Wrote {}: {} tiles, {} polygons, {} restored passage spans",
                             std::string_view(reinterpret_cast<const char*>(OutputName.data()), OutputName.size()), Summary.TileCount,
                             Summary.PolygonCount, Statistics.RestoredPassageSpanCount);
            }
        }
        spdlog::info("[navmesh] Generated {} navmeshes in {:.2f}s. Reload the map to use them; existing NPC graphs were not replaced.", nWrittenCount,
                     g_PlatFloatTime() - flStartTime);
    }
    catch (const std::exception& Error)
    {
        spdlog::error("[navmesh] Generation failed: {}. Unfinished hulls were not published.", Error.what());
    }
}

ON_DLL_LOAD_RELIESON("engine.dll", NavMeshGenerate, (ConCommand), [](CModule)
{
    RegisterConCommand("navmesh_generate", NavMesh_Generate_f,
                       "Build the current local map's .nm files from mounted collision assets. Blocks the server; takes effect after map reload. "
                       "Usage: navmesh_generate [small|med_short|medium|large|all] (default: all four retail MP hulls).",
                       FCVAR_CHEAT | FCVAR_DONTRECORD);
})
