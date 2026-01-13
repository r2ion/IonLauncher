#pragma once

#include "core/macros.h"
#include "engine/r2engine.h"

class CDemoFile;

class CDemoPlayer
{
public:
	virtual ~CDemoPlayer() = 0;
	virtual CDemoFile* GetDemoFile() = 0;
	virtual int GetPlaybackStartTick() = 0;
	virtual int GetPlaybackTick() = 0;
	virtual int GetTotalTicks() = 0;
	virtual bool StartPlayback(const char* filename, bool bAsTimeDemo) = 0;
	virtual bool IsPlayingBack() = 0;
	virtual bool IsPlaybackPaused() = 0;
	virtual bool IsPlayingTimeDemo() = 0;
	virtual bool IsSkipping() = 0;
	virtual int64_t sub_180058040() = 0;
	virtual int64_t sub_1800564E0() = 0;
	virtual void SetPlaybackTimeScale(float timescale) = 0;
	virtual float GetPlaybackTimeScale() = 0;
	virtual void PausePlayback(float seconds) = 0;
	virtual void SkipToTick(int tick, bool bRelative, bool bPause) = 0;
	virtual int64_t ResumePlayback() = 0;
	virtual int64_t StopPlayback() = 0;
	virtual char InterpolateViewpoint() = 0;
	virtual netpacket_s* ReadPacket() = 0;
	virtual void ResetDemoInterpolation() = 0;
};

extern CDemoPlayer* s_ClientDemoPlayer;
