#include "persistence.h"
#include "engine/client.h"

bool NET_SendPersistenceChecksum::ReadFromBuffer(bf_read* buffer)
{
	return true;

}

bool NET_SendPersistenceChecksum::WriteToBuffer(bf_write* buffer)
{
	return true;
}

bool NET_SendPersistenceChecksum::Process(void)
{
	return true;
}

bool CLC_SendPersistentData::ReadFromBuffer(bf_read* buffer)
{
	size_t entries = buffer->ReadLongLong();
	m_persistenceMap.clear();

	for(size_t i = 0; i < entries; ++i)
	{
		char keyBuffer[256];
		char valueBuffer[1024];

		if(!(buffer->ReadString(keyBuffer, sizeof(keyBuffer)) &&
		   buffer->ReadString(valueBuffer, sizeof(valueBuffer))))
		{
			spdlog::error("CLC_SendPersistentData::ReadFromBuffer: Overflowed buffer when reading key-value pair");
			return false;
		}

        m_persistenceMap.emplace(keyBuffer, valueBuffer);
	}

	return true;
}

bool CLC_SendPersistentData::WriteToBuffer(bf_write* buffer)
{
	buffer->WriteUBitLong(this->GetType(), 6);
	buffer->WriteLongLong(static_cast<int64_t>(m_persistenceMap.size()));

	for(const auto& kv : m_persistenceMap)
	{
		const std::string& key = kv.first;
		const std::string& value = kv.second;

		if(!(buffer->WriteString(key.c_str()) &&
		   buffer->WriteString(value.c_str())))
		{
			spdlog::error("CLC_SendPersistentData::WriteToBuffer: Overflowed buffer when writing key-value pair ({} : {})", key, value);
			return false;
		}
	}

	if(buffer->IsOverflowed())
	{
		spdlog::error("CLC_SendPersistentData::WriteToBuffer: Buffer overflowed after writing all key-value pairs");
		return false;
	}

	return true;
}

bool CLC_SendPersistentData::Process(void)
{
	return true;
}

AUTOHOOK_INIT()

AUTOHOOK(CClient__SendSignonData, engine.dll + 0x105760, bool, __fastcall, (CClient* pThis))
{
	return CClient__SendSignonData(pThis);
}

ON_DLL_LOAD_RELIESON("engine.dll", PersistenceNetMessages, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH()
}
