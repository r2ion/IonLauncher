#pragma once

class CNetChan;
class INetMessage;

typedef bool (__fastcall* CNetChan__RegisterMessage_t)(CNetChan* thisptr, INetMessage* msg);
extern CNetChan__RegisterMessage_t CNetChan__RegisterMessage;

