#pragma once

#include "engine/netmessages.h"
#include "util/utils.h"

class NET_SendPersistenceChecksum : public INetMessage
{
public:
	NET_SendPersistenceChecksum() = default;

	NET_SendPersistenceChecksum(const std::map<std::string, std::string>& persistenceMap)
		: m_checksum(CalculateChecksum(persistenceMap))
	{
	}

	virtual bool ReadFromBuffer(bf_read* buffer);
	virtual bool WriteToBuffer(bf_write* buffer);
	virtual bool Process(void);

	virtual int GetType(void) const { return static_cast<int>(NSCustomNetMessages::net_SendPersistenceChecksum); }
	virtual const char* GetName(void) const { return "NET_SendPersistenceChecksum"; }
	virtual const char* ToString(void) const { return "NET_SendPersistenceChecksum"; }
	virtual size_t GetSize(void) const { return sizeof(m_checksum); }

private:
	static std::size_t CalculateChecksum(const std::map<std::string, std::string>& persistenceMap)
	{
		std::size_t seed = 0;

		for (const auto& kv : persistenceMap)
		{
			HashCombine(seed, kv.first);
			HashCombine(seed, kv.second);
		}

		return seed;
	}

	std::size_t m_checksum;
};

class CLC_SendPersistentData : public INetMessage
{
public:
	CLC_SendPersistentData() = default;

	CLC_SendPersistentData(std::map<std::string, std::string>& persistenceMap)
		: m_persistenceMap(persistenceMap)
	{
	}

	virtual bool ReadFromBuffer(bf_read* buffer);
	virtual bool WriteToBuffer(bf_write* buffer);
	virtual bool Process(void);

	virtual int GetType(void) const { return static_cast<int>(NetMessageType::clc_CmdKeyValues); }
	virtual const char* GetName(void) const { return "CLC_SendPersistentData"; }
	virtual const char* ToString(void) const { return "CLC_SendPersistentData"; }
	virtual size_t GetSize(void) const { return sizeof(CLC_SendPersistentData); }

private:
	std::map<std::string, std::string> m_persistenceMap;
};
