#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include "mathlib/color.h"
#include "tier1/iconvar.h"
#include "tier1/utlvector.h"

class ConCommandBase;
class ConVar;

class IConCommandBaseAccessor
{
public:
	virtual bool RegisterConCommandBase(ConCommandBase* command) = 0;
};

class CCommand
{
public:
	CCommand() = delete;
	CCommand(const CCommand& other);

	std::int64_t ArgC() const;
	const char** ArgV() const;
	const char* ArgS() const;
	const char* GetCommandString() const;
	const char* operator[](int index) const;
	const char* Arg(int index) const;

	static int MaxCommandLength();

private:
	enum
	{
		COMMAND_MAX_ARGC = 64,
		COMMAND_MAX_LENGTH = 512,
	};

	std::int64_t m_nArgc;
	std::int64_t m_nArgv0Size;
	char m_pArgSBuffer[COMMAND_MAX_LENGTH];
	char m_pArgvBuffer[COMMAND_MAX_LENGTH];
	const char* m_ppArgv[COMMAND_MAX_ARGC];
};

class ConCommandBase
{
public:
	ConCommandBase() = default;
	virtual ~ConCommandBase(); // 0
	virtual bool IsCommand() const; // 1
	virtual bool IsFlagSet(int flags) const; // 2
	virtual void AddFlags(int flags); // 3
	virtual void RemoveFlags(int flags); // 4
	virtual int GetFlags() const; // 5
	virtual const char* GetName() const; // 6
	virtual const char* GetHelpText() const; // 7
	virtual bool IsRegistered() const; // 8
	virtual int GetDLLIdentifier() const; // 9
	virtual ConCommandBase* Create(const char* name, const char* helpString, int flags); // 10
	virtual void Init(); // 11

	bool HasFlags(int flags) const;
	ConCommandBase* GetNext() const;
	char* CopyString(const char* from) const;

	ConCommandBase* m_pNext {}; // 0x08
	bool m_bRegistered {}; // 0x10
	std::byte m_RegistrationPad[7] {}; // 0x11
	const char* m_pszName {}; // 0x18
	const char* m_pszHelpString {}; // 0x20
	int m_nFlags {}; // 0x28

	static ConCommandBase* s_pConCommandBases;
	static IConCommandBaseAccessor* s_pAccessor;
};

class ConCommand : public ConCommandBase
{
	friend class CCvar;

public:
	ConCommand() = default;
	bool IsCommand() const override;

	FnCommandCallback_t m_pCommandCallback {}; // 0x30
	FnCommandCompletionCallback m_pCompletionCallback {}; // 0x38
	int m_nCallbackFlags {}; // 0x40
	std::uint32_t m_Unknown44 {}; // 0x44
	std::uint64_t m_Unknown48 {}; // 0x48
	std::uint64_t m_Unknown50 {}; // 0x50
};

class ConVar : public ConCommandBase, public IConVar
{
	friend class CCvar;

public:
	ConVar(const char* name, const char* defaultValue, int flags, const char* helpString);
	ConVar(
		const char* name,
		const char* defaultValue,
		int flags,
		const char* helpString,
		bool hasMin,
		float minValue,
		bool hasMax,
		float maxValue,
		FnChangeCallback_t callback);
	~ConVar() override;

	bool IsCommand() const override;
	bool IsFlagSet(int flags) const override;
	void AddFlags(int flags) override;
	void RemoveFlags(int flags) override;
	int GetFlags() const override;
	const char* GetName() const override;
	const char* GetBaseName() const override;
	const char* GetHelpText() const override;
	bool IsRegistered() const override;
	int GetSplitScreenPlayerSlot() const override;

	bool GetBool() const;
	float GetFloat() const;
	int GetInt() const;
	Color GetColor() const;
	const char* GetString() const;

	bool GetMin(float& minValue) const;
	bool GetMax(float& maxValue) const;
	float GetMinValue() const;
	float GetMaxValue() const;
	bool HasMin() const;
	bool HasMax() const;

	void SetValue(int value) override;
	void SetValue(float value) override;
	void SetValue(const char* value) override;
	void SetValue(Color value);

	void ChangeStringValue(const char* value, float oldValue);
	bool SetColorFromString(const char* value);
	bool ClampValue(float& value);

	struct CVValue_t
	{
		const char* m_pszString; // 0x00
		std::int64_t m_iStringLength; // 0x08
		float m_fValue; // 0x10
		int m_nValue; // 0x14
	};

	struct CVChange_t
	{
		bool operator==(const CVChange_t& other) const
		{
			return other.m_pCallback == m_pCallback && !m_pUserData;
		}

		FnChangeCallback_t m_pCallback;
		ChangeUserData_t m_pUserData;
	};

	ConVar* m_pParent {}; // 0x38
	const char* m_pszDefaultValue {}; // 0x40
	CVValue_t m_Value {}; // 0x48
	bool m_bHasMin {}; // 0x60
	std::byte m_MinPad[3] {}; // 0x61
	float m_fMinVal {}; // 0x64
	bool m_bHasMax {}; // 0x68
	std::byte m_MaxPad[3] {}; // 0x69
	float m_fMaxVal {}; // 0x6C
	CUtlVector<CVChange_t> m_fnChangeCallbacks; // 0x70
};

static_assert(sizeof(CCommand) == 0x610);
static_assert(sizeof(ConCommandBase) == 0x30);
static_assert(offsetof(ConCommandBase, m_pNext) == 0x8);
static_assert(offsetof(ConCommandBase, m_bRegistered) == 0x10);
static_assert(offsetof(ConCommandBase, m_pszName) == 0x18);
static_assert(offsetof(ConCommandBase, m_pszHelpString) == 0x20);
static_assert(offsetof(ConCommandBase, m_nFlags) == 0x28);

static_assert(std::is_base_of_v<ConCommandBase, ConCommand>);
static_assert(sizeof(ConCommand) == 0x58);
static_assert(offsetof(ConCommand, m_pCommandCallback) == 0x30);
static_assert(offsetof(ConCommand, m_pCompletionCallback) == 0x38);
static_assert(offsetof(ConCommand, m_nCallbackFlags) == 0x40);
static_assert(offsetof(ConCommand, m_Unknown44) == 0x44);
static_assert(offsetof(ConCommand, m_Unknown48) == 0x48);
static_assert(offsetof(ConCommand, m_Unknown50) == 0x50);

static_assert(std::is_base_of_v<ConCommandBase, ConVar>);
static_assert(std::is_base_of_v<IConVar, ConVar>);
static_assert(sizeof(ConVar::CVValue_t) == 0x18);
static_assert(offsetof(ConVar::CVValue_t, m_pszString) == 0x0);
static_assert(offsetof(ConVar::CVValue_t, m_iStringLength) == 0x8);
static_assert(offsetof(ConVar::CVValue_t, m_fValue) == 0x10);
static_assert(offsetof(ConVar::CVValue_t, m_nValue) == 0x14);
static_assert(sizeof(ConVar::CVChange_t) == 0x10);
static_assert(offsetof(ConVar::CVChange_t, m_pCallback) == 0x0);
static_assert(offsetof(ConVar::CVChange_t, m_pUserData) == 0x8);
static_assert(sizeof(CUtlVector<ConVar::CVChange_t>) == 0x20);
static_assert(sizeof(ConVar) == 0x90);
static_assert(offsetof(ConVar, m_pParent) == 0x38);
static_assert(offsetof(ConVar, m_pParent) == sizeof(ConCommandBase) + sizeof(IConVar));
static_assert(offsetof(ConVar, m_pszDefaultValue) == 0x40);
static_assert(offsetof(ConVar, m_Value) == 0x48);
static_assert(offsetof(ConVar, m_bHasMin) == 0x60);
static_assert(offsetof(ConVar, m_fMinVal) == 0x64);
static_assert(offsetof(ConVar, m_bHasMax) == 0x68);
static_assert(offsetof(ConVar, m_fMaxVal) == 0x6C);
static_assert(offsetof(ConVar, m_fnChangeCallbacks) == 0x70);
