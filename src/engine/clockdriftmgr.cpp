#include "engine/clockdriftmgr.h"
#include "engine/cdll_int.h"
#include "engine/client/clientstate.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"

#include <algorithm>
#include <cmath>
#include <iterator>

DECLARE_MODULE(EngineClockDrift)

ConVar** cl_updaterate_mp;
ConVar** host_timescale;
ConVar** cl_clock_correction_ahead_correct_interval;


ClockDriftSamples g_ClockDriftSamples{};

DECLARE_HOOK(CClockDriftMgr_Clear, engine.dll + 0x11F800, [](auto& hook, CClockDriftMgr* self)
{
    if (g_ClockDriftSamples.owner == self)
        g_ClockDriftSamples = {};
    hook.Original(self);
})

DECLARE_HOOK(CClockDriftMgr_SetServerTick, engine.dll + 0x11F9D0,
             ([](auto& hook, CClockDriftMgr* self, int serverTick)
{
    const float previousServerTime = self->m_lastServerTime;
    const std::uint32_t previousPlatTime = self->m_lastPlatTime;
    const int previousServerTick = self->m_nServerTick;
    const int clockOffsetIndex = self->m_iCurClockOffset;
    hook.Original(self, serverTick);

    const CClientState* const client = GetBaseLocalClient();

	if (!client || self != &client->m_ClockDriftMgr || client->m_nSignonState != eSignonState::FULL || !client->m_NetChannel ||
        client->m_nMaxClients <= 1 || client->m_bIsWatchingReplay || !g_pEngineClient || g_pEngineClient->IsPlayingDemo())
    {
        g_ClockDriftSamples = {};
        return;
    }

    const float serverInterval = self->m_lastServerTime - previousServerTime;
    const float receiveInterval = static_cast<float>(self->m_lastPlatTime - previousPlatTime) * 0.001f * (*host_timescale)->GetFloat();
    const int updateRate = (*cl_updaterate_mp)->GetInt();
    if (previousServerTick <= 0 || serverTick <= previousServerTick || !(serverInterval > 0.0f) || !std::isfinite(serverInterval) ||
        !std::isfinite(receiveInterval) || receiveInterval < 0.0f || updateRate <= 0 || clockOffsetIndex < 0 ||
        clockOffsetIndex >= static_cast<int>(std::size(self->m_ClockOffsets)))
    {
        g_ClockDriftSamples = {};
        return;
    }

    if (g_ClockDriftSamples.owner != self || g_ClockDriftSamples.channel != client->m_NetChannel ||
        g_ClockDriftSamples.serverCount != client->m_nServerCount)
    {
        g_ClockDriftSamples = {self, client->m_NetChannel, client->m_nServerCount};
    }

    const float requestedInterval = 1.0f / static_cast<float>(updateRate);
    const float decay = std::max(0.0f, 1.0f - serverInterval);
    g_ClockDriftSamples.snapshotInterval = std::max(serverInterval, g_ClockDriftSamples.snapshotInterval * decay);
    g_ClockDriftSamples.jitter = std::max(std::fabs(receiveInterval - serverInterval), g_ClockDriftSamples.jitter * decay);

    const float previousExtraDelay = g_ClockDriftSamples.extraDelay;
    const float interval = std::max(requestedInterval, g_ClockDriftSamples.snapshotInterval) + g_ClockDriftSamples.jitter;
    const float extraDelay = std::clamp(interval - requestedInterval, 0.0f, 0.1f);
    g_ClockDriftSamples.extraDelay = extraDelay;
    if (extraDelay == 0.0f && previousExtraDelay == 0.0f)
        return;

    const bool reset = self->m_iCurClockOffset == clockOffsetIndex;
    const float offset = (reset ? 0.0f : self->m_ClockOffsets[clockOffsetIndex]) + extraDelay;
    for (int i = 0; i < static_cast<int>(std::size(self->m_ClockOffsets)); ++i)
    {
        if (reset || i == clockOffsetIndex)
            self->m_ClockOffsets[i] = offset;
        else
            self->m_ClockOffsets[i] += extraDelay - previousExtraDelay;
    }
    self->m_aheadBy = std::max(0.0f, offset);
    if (self->m_aheadBy > 0.0f)
        self->m_correctWithin = std::max(requestedInterval + extraDelay, (*cl_clock_correction_ahead_correct_interval)->GetFloat() * 0.001f);
}))

ON_DLL_LOAD_CLIENT("engine.dll", ClockDriftMethods, [](CModule module)
{
    cl_updaterate_mp = module.Offset(0xFDA5A38).RCast<ConVar**>();
    host_timescale = module.Offset(0x1315A2A8).RCast<ConVar**>();
    cl_clock_correction_ahead_correct_interval = module.Offset(0x130D98B8).RCast<ConVar**>();
    DISPATCH_MODULE(EngineClockDrift)
})
