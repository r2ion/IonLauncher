#include "client/player.h"
#include "tier0/callbacks.h"

using CPlayerIsMantlingFn = bool (*)(const C_Player*);
using CPlayerGetLocalPlayerFn = C_Player* (*)(int);
CPlayerGetLocalPlayerFn C_Player__GetLocalPlayer;
CPlayerGetLocalPlayerFn C_Player__GetLocalViewPlayer;
CPlayerIsMantlingFn C_Player__IsMantling;

C_Player* C_Player::GetLocalViewPlayer(const int splitScreenSlot)
{
    return C_Player__GetLocalViewPlayer(splitScreenSlot);
}

C_Player* C_Player::GetLocalPlayer(const int splitScreenSlot)
{
    return C_Player__GetLocalPlayer(splitScreenSlot);
}

bool C_Player::IsMantling() const
{
    return C_Player__IsMantling(this);
}

ON_DLL_LOAD_CLIENT("client.dll", ClientPlayerMethods, [](CModule module)
{
    C_Player__GetLocalViewPlayer = module.Offset(0x14EF00).RCast<CPlayerGetLocalPlayerFn>();
    C_Player__GetLocalPlayer = module.Offset(0x14EF40).RCast<CPlayerGetLocalPlayerFn>();
    C_Player__IsMantling = module.Offset(0x9E0B0).RCast<CPlayerIsMantlingFn>();
})
