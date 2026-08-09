#pragma once

#include <cstddef>
#include <cstdint>

#include "tier1/utlvector.h"

class bf_read;
class bf_write;
class CNetworkStringDict;
class INetworkStringTable;

using TABLEID = int;
using NetworkStringChangedFn = void (*)(void* pObject, INetworkStringTable* pStringTable, int stringIndex,
	const char* pNewString, const void* pNewData);

inline constexpr TABLEID INVALID_STRING_TABLE = -1;
inline constexpr std::uint16_t INVALID_STRING_INDEX = static_cast<std::uint16_t>(-1);
inline constexpr int MAX_NETWORK_STRING_TABLES = 32;
inline constexpr char VENGINE_CLIENT_STRING_TABLE_INTERFACE_VERSION[] = "VEngineClientStringTable001";
inline constexpr char VENGINE_SERVER_STRING_TABLE_INTERFACE_VERSION[] = "VEngineServerStringTable001";

class INetworkStringTable
{
public:
	virtual ~INetworkStringTable() = default;
	virtual const char* GetTableName() const = 0;
	virtual TABLEID GetTableId() const = 0;
	virtual int GetNumStrings() const = 0;
	virtual int GetMaxStrings() const = 0;
	virtual int GetEntryBits() const = 0;
	virtual void SetTick(int tick) = 0;
	virtual bool ChangedSinceTick(int tick) const = 0;
	virtual int AddString(bool isServer, const char* pValue, int length = -1, const void* pUserData = nullptr) = 0;
	virtual const char* GetString(int stringIndex) = 0;
	virtual void SetStringUserData(int stringIndex, unsigned int length, const void* pUserData) = 0;
	virtual const void* GetStringUserData(int stringIndex, int* pLength) = 0;
	virtual int FindStringIndex(const char* pValue) = 0;
	virtual bool WriteStringTable(bf_write* pBuffer) = 0;
	virtual bool ReadStringTable(bf_read* pBuffer) = 0;
	virtual void SetStringChangedCallback(void* pObject, NetworkStringChangedFn callback) = 0;
	virtual void PurgeAllClientSide() = 0;
	virtual void EnableRollback(bool enabled) = 0;
};

class INetworkStringTableContainer
{
public:
	virtual ~INetworkStringTableContainer() = default;
	virtual INetworkStringTable* CreateStringTable(const char* pTableName, int maxEntries,
		int userDataFixedSize = 0, int userDataNetworkBits = 0, int dictionaryFlags = 0) = 0;
	virtual void RemoveAllTables() = 0;
	virtual INetworkStringTable* FindTable(const char* pTableName) const = 0;
	virtual INetworkStringTable* GetTable(TABLEID tableId) const = 0;
	virtual int GetNumTables() const = 0;
	virtual void SetAllowClientSideAddString(INetworkStringTable* pTable, bool allowClientSideAddString) = 0;
	virtual void BuildMapStringDictionary(const char* pMapName) = 0;
};

class CNetworkStringTable : public INetworkStringTable
{
public:
	CNetworkStringTable(TABLEID tableId, const char* pTableName, int maxEntries,
		int userDataFixedSize, int userDataNetworkBits, int dictionaryFlags);
	~CNetworkStringTable() override;

	const char* GetTableName() const override;
	TABLEID GetTableId() const override;
	int GetNumStrings() const override;
	int GetMaxStrings() const override;
	int GetEntryBits() const override;
	void SetTick(int tick) override;
	bool ChangedSinceTick(int tick) const override;
	int AddString(bool isServer, const char* pValue, int length = -1, const void* pUserData = nullptr) override;
	const char* GetString(int stringIndex) override;
	void SetStringUserData(int stringIndex, unsigned int length, const void* pUserData) override;
	const void* GetStringUserData(int stringIndex, int* pLength) override;
	int FindStringIndex(const char* pValue) override;
	bool WriteStringTable(bf_write* pBuffer) override;
	bool ReadStringTable(bf_read* pBuffer) override;
	void SetStringChangedCallback(void* pObject, NetworkStringChangedFn callback) override;
	void PurgeAllClientSide() override;
	void EnableRollback(bool enabled) override;

	TABLEID m_TableId; // 0x0008
	char* m_TableName; // 0x0010
	int m_MaxEntries; // 0x0018
	int m_EntryBits; // 0x001C
	int m_TickCount; // 0x0020
	int m_LastChangedTick; // 0x0024
	std::uint32_t m_Flags; // 0x0028
	std::uint32_t m_DictionaryFlags; // 0x002C
	int m_UserDataFixedSize; // 0x0030
	int m_UserDataNetworkBits; // 0x0034
	NetworkStringChangedFn m_StringChangedCallback; // 0x0038
	void* m_CallbackObject; // 0x0040
	CNetworkStringTable* m_MirrorTable; // 0x0048
	CNetworkStringDict* m_ServerDictionary; // 0x0050
	CNetworkStringDict* m_ClientDictionary; // 0x0058
};

static_assert(sizeof(CNetworkStringTable) == 0x60);
static_assert(offsetof(CNetworkStringTable, m_TableId) == 0x08);
static_assert(offsetof(CNetworkStringTable, m_TableName) == 0x10);
static_assert(offsetof(CNetworkStringTable, m_MaxEntries) == 0x18);
static_assert(offsetof(CNetworkStringTable, m_EntryBits) == 0x1C);
static_assert(offsetof(CNetworkStringTable, m_TickCount) == 0x20);
static_assert(offsetof(CNetworkStringTable, m_LastChangedTick) == 0x24);
static_assert(offsetof(CNetworkStringTable, m_Flags) == 0x28);
static_assert(offsetof(CNetworkStringTable, m_DictionaryFlags) == 0x2C);
static_assert(offsetof(CNetworkStringTable, m_UserDataFixedSize) == 0x30);
static_assert(offsetof(CNetworkStringTable, m_UserDataNetworkBits) == 0x34);
static_assert(offsetof(CNetworkStringTable, m_StringChangedCallback) == 0x38);
static_assert(offsetof(CNetworkStringTable, m_CallbackObject) == 0x40);
static_assert(offsetof(CNetworkStringTable, m_MirrorTable) == 0x48);
static_assert(offsetof(CNetworkStringTable, m_ServerDictionary) == 0x50);
static_assert(offsetof(CNetworkStringTable, m_ClientDictionary) == 0x58);

class CNetworkStringTableContainer : public INetworkStringTableContainer
{
public:
	CNetworkStringTableContainer();
	~CNetworkStringTableContainer() override;

	INetworkStringTable* CreateStringTable(const char* pTableName, int maxEntries,
		int userDataFixedSize = 0, int userDataNetworkBits = 0, int dictionaryFlags = 0) override;
	void RemoveAllTables() override;
	INetworkStringTable* FindTable(const char* pTableName) const override;
	INetworkStringTable* GetTable(TABLEID tableId) const override;
	int GetNumTables() const override;
	void SetAllowClientSideAddString(INetworkStringTable* pTable, bool allowClientSideAddString) override;
	void BuildMapStringDictionary(const char* pMapName) override;

	bool m_AllowCreation; // 0x0008
	int m_TickCount; // 0x000C
	bool m_Locked; // 0x0010
	bool m_EnableRollback; // 0x0011
	CUtlVector<CNetworkStringTable*> m_Tables; // 0x0018
};

static_assert(sizeof(CNetworkStringTableContainer) == 0x38);
static_assert(offsetof(CNetworkStringTableContainer, m_AllowCreation) == 0x08);
static_assert(offsetof(CNetworkStringTableContainer, m_TickCount) == 0x0C);
static_assert(offsetof(CNetworkStringTableContainer, m_Locked) == 0x10);
static_assert(offsetof(CNetworkStringTableContainer, m_EnableRollback) == 0x11);
static_assert(offsetof(CNetworkStringTableContainer, m_Tables) == 0x18);
