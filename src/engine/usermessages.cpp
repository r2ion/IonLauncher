#include "engine/usermessages.h"

CUserMessageManager* g_pUserMessageManager;

typedef void (__fastcall*CUserMessages__Register_t)(void* thisptr, const char* pszName, unsigned int uiSize);
CUserMessages__Register_t CUserMessages__Register = nullptr;
typedef void (__fastcall*CUserMessages__HookMessage_t)(void* thisptr, const char* pszName, void* pCallback);
CUserMessages__HookMessage_t CUserMessages__HookMessage = nullptr;
uintptr_t usermessages = 0;

void CUserMessageManager::RegisterUserMessages()
{
	for(const auto& [name, size] : m_UserMessages)
	{
		CUserMessages__Register(reinterpret_cast<void*>(usermessages), name, size);
	}
}

void CUserMessageManager::Register(const char* pszName, unsigned int uiSize)
{
	m_UserMessages.emplace_back(pszName, uiSize);
}

void CUserMessageManager::HookMessage(const char* pszName, void* pCallback)
{
	CUserMessages__HookMessage(reinterpret_cast<void*>(usermessages), pszName, pCallback);
}

AUTOHOOK_INIT()

AUTOHOOK(RegisterUserMessages, client.dll + 0x49E620, void, __fastcall, ())
{
	RegisterUserMessages();
	g_pUserMessageManager->RegisterUserMessages();
}

ON_DLL_LOAD_CLIENT("client.dll", UserMessages, (CModule module))
{
	CUserMessages__Register = module.Offset(0x342890).RCast<CUserMessages__Register_t>();
	usermessages = module.Offset(0xB28E98).GetPtr();

	AUTOHOOK_DISPATCH()
}
