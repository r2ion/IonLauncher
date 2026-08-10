#include "concommand.h"
#include "engine/shared/misccommands.h"
#include "engine/r2engine.h"

#include <cstddef>
#include <cstring>

ConCommandBase* ConCommandBase::s_pConCommandBases = nullptr;
IConCommandBaseAccessor* ConCommandBase::s_pAccessor = nullptr;

static int s_ConVarDLLIdentifier = -1;

CCommand::CCommand(const CCommand& other)
{
	m_nArgc = other.m_nArgc;
	m_nArgv0Size = other.m_nArgv0Size;
	std::memcpy(m_pArgSBuffer, other.m_pArgSBuffer, sizeof(m_pArgSBuffer));
	std::memcpy(m_pArgvBuffer, other.m_pArgvBuffer, sizeof(m_pArgvBuffer));

	for (int i = 0; i < COMMAND_MAX_ARGC; ++i)
	{
		if (!other.m_ppArgv[i])
		{
			m_ppArgv[i] = nullptr;
			continue;
		}

		const std::ptrdiff_t offset = other.m_ppArgv[i] - other.m_pArgvBuffer;
		m_ppArgv[i] = offset >= 0 && offset < COMMAND_MAX_LENGTH ? m_pArgvBuffer + offset : nullptr;
	}
}

std::int64_t CCommand::ArgC() const
{
	return m_nArgc;
}

const char** CCommand::ArgV() const
{
	return m_nArgc ? const_cast<const char**>(m_ppArgv) : nullptr;
}

const char* CCommand::ArgS() const
{
	return m_nArgv0Size ? &m_pArgSBuffer[m_nArgv0Size] : "";
}

const char* CCommand::GetCommandString() const
{
	return m_nArgc ? m_pArgSBuffer : "";
}

const char* CCommand::Arg(int index) const
{
	return index >= 0 && index < m_nArgc ? m_ppArgv[index] : "";
}

const char* CCommand::operator[](int index) const
{
	return Arg(index);
}

int CCommand::MaxCommandLength()
{
	return COMMAND_MAX_LENGTH - 1;
}

ConCommandBase::~ConCommandBase() = default;

bool ConCommandBase::IsCommand() const
{
	return true;
}

bool ConCommandBase::IsFlagSet(int flags) const
{
	return (m_nFlags & flags) != 0;
}

void ConCommandBase::AddFlags(int flags)
{
	m_nFlags |= flags;
}

void ConCommandBase::RemoveFlags(int flags)
{
	m_nFlags &= ~flags;
}

int ConCommandBase::GetFlags() const
{
	return m_nFlags;
}

const char* ConCommandBase::GetName() const
{
	return m_pszName;
}

const char* ConCommandBase::GetHelpText() const
{
	return m_pszHelpString;
}

bool ConCommandBase::IsRegistered() const
{
	return m_bRegistered;
}

int ConCommandBase::GetDLLIdentifier() const
{
	return s_ConVarDLLIdentifier;
}

ConCommandBase* ConCommandBase::Create(const char* name, const char* helpString, int flags)
{
	m_bRegistered = false;
	m_pszName = name;
	m_pszHelpString = helpString ? helpString : "";
	m_nFlags = flags;

	if (flags & FCVAR_UNREGISTERED)
	{
		m_pNext = nullptr;
	}
	else
	{
		m_pNext = s_pConCommandBases;
		s_pConCommandBases = this;
	}

	if (s_pAccessor)
		Init();

	return this;
}

void ConCommandBase::Init()
{
	if (s_pAccessor)
		m_bRegistered = s_pAccessor->RegisterConCommandBase(this);
}

bool ConCommandBase::HasFlags(int flags) const
{
	return (m_nFlags & flags) != 0;
}

ConCommandBase* ConCommandBase::GetNext() const
{
	return m_pNext;
}

char* ConCommandBase::CopyString(const char* from) const
{
	const std::size_t length = std::strlen(from);
	char* copy = new char[length + 1];
	std::memcpy(copy, from, length + 1);
	return copy;
}

bool ConCommand::IsCommand() const
{
	return true;
}

using ConCommandConstructorType =
	void (*)(ConCommand* command, const char* name, FnCommandCallback_t callback, const char* helpString, int flags, void* parent);
static ConCommandConstructorType s_ConCommandConstructor;

void RegisterConCommand(const char* name, FnCommandCallback_t callback, const char* helpString, int flags)
{
	spdlog::info("Registering ConCommand {}", name);
	ConCommand* command = new ConCommand;
	s_ConCommandConstructor(command, name, callback, helpString, flags, nullptr);
}

void RegisterConCommand(
	const char* name,
	FnCommandCallback_t callback,
	const char* helpString,
	int flags,
	FnCommandCompletionCallback completionCallback)
{
	spdlog::info("Registering ConCommand {}", name);
	ConCommand* command = new ConCommand;
	s_ConCommandConstructor(command, name, callback, helpString, flags, nullptr);
	command->m_pCompletionCallback = completionCallback;
	command->m_nCallbackFlags |= 0x3;
}

ON_DLL_LOAD("engine.dll", ConCommand, [](CModule module)
{
	s_ConCommandConstructor = module.Offset(0x415F60).RCast<ConCommandConstructorType>();
	AddMiscConCommands();
})
