#pragma once
#include "shared/keyvalues.h"
#include "engine/bitbuf.h"
#include "engine/net.h"
#include "engine/netmessages.h"

// Cbuf
enum class ECommandTarget_t
{
	CBUF_FIRST_PLAYER = 0,
	CBUF_LAST_PLAYER = 1, // MAX_SPLITSCREEN_CLIENTS - 1, MAX_SPLITSCREEN_CLIENTS = 2
	CBUF_SERVER = CBUF_LAST_PLAYER + 1,

	CBUF_COUNT,
};

enum class cmd_source_t
{
	// Added to the console buffer by gameplay code.  Generally unrestricted.
	kCommandSrcCode,

	// Sent from code via engine->ClientCmd, which is restricted to commands visible
	// via FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS.
	kCommandSrcClientCmd,

	// Typed in at the console or via a user key-bind.  Generally unrestricted, although
	// the client will throttle commands sent to the server this way to 16 per second.
	kCommandSrcUserInput,

	// Came in over a net connection as a clc_stringcmd
	// host_client will be valid during this state.
	//
	// Restricted to FCVAR_GAMEDLL commands (but not convars) and special non-ConCommand
	// server commands hardcoded into gameplay code (e.g. "joingame")
	kCommandSrcNetClient,

	// Received from the server as the client
	//
	// Restricted to commands with FCVAR_SERVER_CAN_EXECUTE
	kCommandSrcNetServer,

	// Being played back from a demo file
	//
	// Not currently restricted by convar flag, but some commands manually ignore calls
	// from this source.  FIXME: Should be heavily restricted as demo commands can come
	// from untrusted sources.
	kCommandSrcDemoFile,

	// Invalid value used when cleared
	kCommandSrcInvalid = -1
};

typedef ECommandTarget_t (*Cbuf_GetCurrentPlayerType)();
extern Cbuf_GetCurrentPlayerType Cbuf_GetCurrentPlayer;

typedef void (*Cbuf_AddTextType)(ECommandTarget_t eTarget, const char* text, cmd_source_t source);
extern Cbuf_AddTextType Cbuf_AddText;

typedef void (*Cbuf_ExecuteType)();
extern Cbuf_ExecuteType Cbuf_Execute;

extern bool (*CCommand__Tokenize)(CCommand& self, const char* pCommandString, cmd_source_t commandSource);

// CEngine

enum EngineQuitState
{
	QUIT_NOTQUITTING = 0,
	QUIT_TODESKTOP,
	QUIT_RESTART
};

enum class EngineState_t
{
	DLL_INACTIVE = 0, // no dll
	DLL_ACTIVE, // engine is focused
	DLL_CLOSE, // closing down dll
	DLL_RESTART, // engine is shutting down but will restart right away
	DLL_PAUSED, // engine is paused, can become active from this state
};

class CEngine
{
public:
	virtual void unknown() = 0; // unsure if this is where
	virtual bool Load(bool dedicated, const char* baseDir) = 0;
	virtual void Unload() = 0;
	virtual void SetNextState(EngineState_t iNextState) = 0;
	virtual EngineState_t GetState() = 0;
	virtual void Frame() = 0;
	virtual double GetFrameTime() = 0;
	virtual float GetCurTime() = 0;

	EngineQuitState m_nQuitting;
	EngineState_t m_nDllState;
	EngineState_t m_nNextDllState;
	double m_flCurrentTime;
	float m_flFrameTime;
	double m_flPreviousTime;
	float m_flFilteredTime;
	float m_flMinFrameTime; // Expected duration of a frame, or zero if it is unlimited.
};

extern CEngine* g_pEngine;

enum server_state_t
{
	ss_dead = 0, // Dead
	ss_loading, // Spawning
	ss_active, // Running
	ss_paused, // Running, but paused
};

extern server_state_t* g_pServerState;

extern char* g_pModName;

enum class GameMode_t : int
{
	NO_MODE = 0,
	MP_MODE,
	SP_MODE,
};

class CGlobalVars
{
public:
	// Absolute time (per frame still - Use Plat_FloatTime() for a high precision real time
	//  perf clock, but not that it doesn't obey host_timescale/host_framerate)
	double m_flRealTime; // 0x0 ( Size: 8 )

	// Absolute frame counter - continues to increase even if game is paused
	int m_nFrameCount; // 0x8 ( Size: 4 )

	// Non-paused frametime
	float m_flAbsoluteFrameTime; // 0xc ( Size: 4 )

	// Current time
	//
	// On the client, this (along with tickcount) takes a different meaning based on what
	// piece of code you're in:
	//
	//   - While receiving network packets (like in PreDataUpdate/PostDataUpdate and proxies),
	//     this is set to the SERVER TICKCOUNT for that packet. There is no interval between
	//     the server ticks.
	//     [server_current_Tick * tick_interval]
	//
	//   - While rendering, this is the exact client clock
	//     [client_current_tick * tick_interval + interpolation_amount]
	//
	//   - During prediction, this is based on the client's current tick:
	//     [client_current_tick * tick_interval]
	float m_flCurTime; // 0x10 ( Size: 4 )

	char _unk_0x14[28]; // 0x14 ( Size: 28 )

	// Time spent on last server or client frame (has nothing to do with think intervals)
	float m_flFrameTime; // 0x30 ( Size: 4 )

	// current maxplayers setting
	int m_nMaxClients; // 0x34 ( Size: 4 )
	GameMode_t m_nGameMode; // 0x38 ( Size: 4 )

	// Simulation ticks - does not increase when game is paused
	//   this is weird and doesn't seem to increase once per frame?
	uint32_t m_nTickCount; // 0x3c ( Size: 4 )

	// Simulation tick interval
	float m_flTickInterval; // 0x40 ( Size: 4 )
	char _unk_0x44[28]; // 0x44 ( Size: 28 )

	const char* m_pMapName; // 0x60 ( Size: 8 )
	int m_nMapVersion; // 0x68 ( Size: 4 )
};
static_assert(offsetof(CGlobalVars, m_flRealTime) == 0x0);
static_assert(offsetof(CGlobalVars, m_nFrameCount) == 0x8);
static_assert(offsetof(CGlobalVars, m_flAbsoluteFrameTime) == 0xc);
static_assert(offsetof(CGlobalVars, m_flCurTime) == 0x10);
static_assert(offsetof(CGlobalVars, m_flFrameTime) == 0x30);
static_assert(offsetof(CGlobalVars, m_nMaxClients) == 0x34);
static_assert(offsetof(CGlobalVars, m_nGameMode) == 0x38);
static_assert(offsetof(CGlobalVars, m_nTickCount) == 0x3c);
static_assert(offsetof(CGlobalVars, m_flTickInterval) == 0x40);
static_assert(offsetof(CGlobalVars, m_pMapName) == 0x60);
static_assert(offsetof(CGlobalVars, m_nMapVersion) == 0x68);

extern CGlobalVars* g_pGlobals;
