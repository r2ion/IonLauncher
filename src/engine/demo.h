#pragma once

#include "engine/net.h"
#include "engine/r2engine.h"

class CDemoFile;

class IDemoRecorder
{
public:
	virtual ~IDemoRecorder() = default;
	virtual CDemoFile* GetDemoFile() = 0;
	virtual int GetRecordingTick() = 0;
	virtual void StartRecording(const char* filename) = 0;
	virtual void SetSignonState(int state, bool wasFullUpdate) = 0;
	virtual bool IsRecording() = 0;
private:
	virtual void Reserved006() = 0;
	virtual void Reserved007() = 0;
public:
	virtual void StopRecording() = 0;
	virtual void RecordCommand(const char* command) = 0;
	virtual void RecordUserInput(int commandNumber) = 0;
private:
	virtual void Reserved011() = 0;
	virtual void Reserved012() = 0;
	virtual void Reserved013() = 0;
	virtual void Reserved014() = 0;
	virtual void Reserved015() = 0;
	virtual void Reserved016() = 0;
	virtual void Reserved017() = 0;
	virtual void Reserved018() = 0;
	virtual void Reserved019() = 0;
	virtual void Reserved020() = 0;
};

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
	virtual netpacket_t* ReadPacket() = 0;
	virtual void ResetDemoInterpolation() = 0;
};

extern CDemoPlayer* g_pDemoPlayer;
extern IDemoRecorder* g_pDemoRecorder;
