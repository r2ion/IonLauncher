#include "engine/usermessages.h"

#include "tier0/module.h"

CUserMessages* usermessages = nullptr;

using LookupUserMessage_t = int (*)(CUserMessages*, const char*);
using GetUserMessageSize_t = int (*)(CUserMessages*, int);
using GetUserMessageName_t = const char* (*)(CUserMessages*, int);
using RegisterUserMessage_t = void (*)(CUserMessages*, const char*, int);
using HookUserMessage_t = void (*)(CUserMessages*, const char*, pfnUserMsgHook);
using DispatchUserMessage_t = bool (*)(CUserMessages*, int, bf_read&);

LookupUserMessage_t CUserMessages__LookupUserMessage;
GetUserMessageSize_t CUserMessages__GetUserMessageSize;
GetUserMessageName_t CUserMessages_GetUserMessageName;
RegisterUserMessage_t CUserMessages__RegisterUserMessage;
HookUserMessage_t CUserMessages__HookUserMessage;
DispatchUserMessage_t CUserMessages__DispatchUserMessage;

CUserMessages::CUserMessages() = default;

CUserMessages::~CUserMessages()
{
    const int count = m_UserMessages.Count();
    for (int i = 0; i < count; ++i)
        delete m_UserMessages[i];

    m_UserMessages.RemoveAll();
}

int CUserMessages::LookupUserMessage(const char* name)
{
    return CUserMessages__LookupUserMessage(this, name);
}

int CUserMessages::GetUserMessageSize(int index)
{
    return CUserMessages__GetUserMessageSize(this, index);
}

const char* CUserMessages::GetUserMessageName(int index)
{
    return CUserMessages_GetUserMessageName(this, index);
}

bool CUserMessages::IsValidIndex(int index)
{
    return m_UserMessages.IsValidIndex(index);
}

void CUserMessages::Register(const char* name, int size)
{
    CUserMessages__RegisterUserMessage(this, name, size);
}

void CUserMessages::HookMessage(const char* name, pfnUserMsgHook hook)
{
    CUserMessages__HookUserMessage(this, name, hook);
}

bool CUserMessages::DispatchUserMessage(int msgType, bf_read& msgData)
{
    return CUserMessages__DispatchUserMessage(this, msgType, msgData);
}

ON_DLL_LOAD_CLIENT("client.dll", UserMessages, [](CModule module)
{
    usermessages = *module.Offset(0xB28E98).RCast<CUserMessages**>();

    CUserMessages__DispatchUserMessage = module.Offset(0x340D20).RCast<DispatchUserMessage_t>();
    CUserMessages_GetUserMessageName = module.Offset(0x341250).RCast<GetUserMessageName_t>();
    CUserMessages__GetUserMessageSize = module.Offset(0x3412A0).RCast<GetUserMessageSize_t>();
    CUserMessages__HookUserMessage = module.Offset(0x3415F0).RCast<HookUserMessage_t>();
    CUserMessages__LookupUserMessage = module.Offset(0x342340).RCast<LookupUserMessage_t>();
    CUserMessages__RegisterUserMessage = module.Offset(0x342890).RCast<RegisterUserMessage_t>();
})
