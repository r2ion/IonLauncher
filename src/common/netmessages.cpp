#include "common/netmessages.h"
#include "tier0/callbacks.h"

#include <cstdio>

NET_SignonState::NET_SignonState()
	: m_nSignonState(eSignonState::NONE),
	  m_nSpawnCount(0),
	  m_szMapName{},
	  m_szGameMode{},
	  m_nPlaylistVersion(0),
	  m_bSendPlaylists(false),
	  m_szPlaylistName{}
{
	m_nGroup = 0;
	m_bReliable = true;
	m_NetChannel = nullptr;
	m_pMessageHandler = nullptr;
}

NET_SignonState::NET_SignonState(eSignonState state, std::int32_t spawnCount)
	: NET_SignonState()
{
	m_nSignonState = state;
	m_nSpawnCount = spawnCount;
}

bool NET_SignonState::Process()
{
	return m_pMessageHandler->ProcessSignonState(this);
}

bool NET_SignonState::ReadFromBuffer(bf_read* buffer)
{
	m_nSignonState = static_cast<eSignonState>(buffer->ReadByte());
	m_nSpawnCount = buffer->ReadLong();
	buffer->ReadString(m_szMapName, sizeof(m_szMapName));
	buffer->ReadString(m_szGameMode, sizeof(m_szGameMode));
	m_bSendPlaylists = buffer->ReadByte() != 0;
	m_nPlaylistVersion = buffer->ReadLong();
	buffer->ReadString(m_szPlaylistName, sizeof(m_szPlaylistName));
	return !buffer->IsOverflowed();
}

bool NET_SignonState::WriteToBuffer(bf_write* buffer)
{
	buffer->WriteByte(static_cast<int>(m_nSignonState));
	buffer->WriteLong(m_nSpawnCount);
	buffer->WriteString(m_szMapName);
	buffer->WriteString(m_szGameMode);
	buffer->WriteByte(m_bSendPlaylists);
	buffer->WriteLong(m_nPlaylistVersion);
	buffer->WriteString(m_szPlaylistName);
	return !buffer->IsOverflowed();
}

const char* NET_SignonState::ToString() const
{
	static char text[1024];
	std::snprintf(text, sizeof(text), "%s: state %i, count %i", GetName(), static_cast<int>(m_nSignonState), m_nSpawnCount);
	return text;
}

using ClientTickProcess_t = bool (*)(CLC_ClientTick*);
using ClientTickRead_t = bool (*)(CLC_ClientTick*, bf_read*);
using ClientTickWrite_t = bool (*)(CLC_ClientTick*, bf_write*);
using ClientTickToString_t = const char* (*)(const CLC_ClientTick*);

ClientTickProcess_t CLC_ClientTick__Process;
ClientTickRead_t CLC_ClientTick__ReadFromBuffer;
ClientTickWrite_t CLC_ClientTick__WriteToBuffer;
ClientTickToString_t CLC_ClientTick__ToString;

CLC_ClientTick::CLC_ClientTick()
	: m_nDeltaTick(0), m_nStringTableTick(0), m_flFrameTime(0), m_flFrameTimeStdDeviation(0), m_nServerCPU(0)
{
	m_nGroup = 0;
	m_bReliable = false;
	m_NetChannel = nullptr;
	m_pMessageHandler = nullptr;
}

bool CLC_ClientTick::Process()
{
	return CLC_ClientTick__Process(this);
}

bool CLC_ClientTick::ReadFromBuffer(bf_read* buffer)
{
	return CLC_ClientTick__ReadFromBuffer(this, buffer);
}

bool CLC_ClientTick::WriteToBuffer(bf_write* buffer)
{
	return CLC_ClientTick__WriteToBuffer(this, buffer);
}

const char* CLC_ClientTick::ToString() const
{
	return CLC_ClientTick__ToString(this);
}

ON_DLL_LOAD_CLIENT("engine.dll", ClientTickMessageMethods, [](CModule module)
{
	CLC_ClientTick__Process = module.Offset(0x75CC0).RCast<ClientTickProcess_t>();
	CLC_ClientTick__ReadFromBuffer = module.Offset(0x220AF0).RCast<ClientTickRead_t>();
	CLC_ClientTick__WriteToBuffer = module.Offset(0x22AA80).RCast<ClientTickWrite_t>();
	CLC_ClientTick__ToString = module.Offset(0x229540).RCast<ClientTickToString_t>();
})
