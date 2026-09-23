#include "server/usermessages.h"

#include "irecipientfilter.h"
#include "mathlib/vector.h"
#include "tier0/module.h"

using UserMessageBeginFn = void(__fastcall*)(IRecipientFilter* recipients, const char* messageName, MessageReplayInteraction replayInteraction);
using MessageEndFn = void(__fastcall*)();
using MessageWriteAngleFn = void(__fastcall*)(float value);
using MessageWriteAnglesFn = void(__fastcall*)(const QAngle& value);
using MessageWriteBitVecIntegralFn = void(__fastcall*)(const Vector3D& value);
using MessageWriteBitsFn = void(__fastcall*)(const void* data, int bitCount);
using MessageWriteBoolFn = void(__fastcall*)(bool value);
using MessageWriteByteFn = void(__fastcall*)(int value);
using MessageWriteCharFn = void(__fastcall*)(int value);
using MessageWriteCoordFn = void(__fastcall*)(float value);
using MessageWriteEHandleFn = void(__fastcall*)(CBaseEntity* entity);
using MessageWriteEntityFn = void(__fastcall*)(int value);
using MessageWriteFloatFn = void(__fastcall*)(float value);
using MessageWriteLongFn = void(__fastcall*)(std::int32_t value);
using MessageWriteSBitLongFn = void(__fastcall*)(std::int32_t value, int bitCount);
using MessageWriteShortFn = void(__fastcall*)(int value);
using MessageWriteStringFn = void(__fastcall*)(const char* value);
using MessageWriteUBitLongFn = void(__fastcall*)(std::uint32_t value, int bitCount);
using MessageWriteVec3CoordFn = void(__fastcall*)(const Vector3D& value);
using MessageWriteVec3NormalFn = void(__fastcall*)(const Vector3D& value);
using MessageWriteWordFn = void(__fastcall*)(int value);

UserMessageBeginFn s_UserMessageBegin;
MessageEndFn s_MessageEnd;
MessageWriteAngleFn s_MessageWriteAngle;
MessageWriteAnglesFn s_MessageWriteAngles;
MessageWriteBitVecIntegralFn s_MessageWriteBitVecIntegral;
MessageWriteBitsFn s_MessageWriteBits;
MessageWriteBoolFn s_MessageWriteBool;
MessageWriteByteFn s_MessageWriteByte;
MessageWriteCharFn s_MessageWriteChar;
MessageWriteCoordFn s_MessageWriteCoord;
MessageWriteEHandleFn s_MessageWriteEHandle;
MessageWriteEntityFn s_MessageWriteEntity;
MessageWriteFloatFn s_MessageWriteFloat;
MessageWriteLongFn s_MessageWriteLong;
MessageWriteSBitLongFn s_MessageWriteSBitLong;
MessageWriteShortFn s_MessageWriteShort;
MessageWriteStringFn s_MessageWriteString;
MessageWriteUBitLongFn s_MessageWriteUBitLong;
MessageWriteVec3CoordFn s_MessageWriteVec3Coord;
MessageWriteVec3NormalFn s_MessageWriteVec3Normal;
MessageWriteWordFn s_MessageWriteWord;

void UserMessageBegin(IRecipientFilter& recipients, const char* messageName, const MessageReplayInteraction replayInteraction)
{
    s_UserMessageBegin(&recipients, messageName, replayInteraction);
}

void MessageEnd()
{
    s_MessageEnd();
}

void MessageWriteAngle(const float value)
{
    s_MessageWriteAngle(value);
}

void MessageWriteAngles(const QAngle& value)
{
    s_MessageWriteAngles(value);
}

void MessageWriteBitVecIntegral(const Vector3D& value)
{
    s_MessageWriteBitVecIntegral(value);
}

void MessageWriteBits(const void* data, const int bitCount)
{
    s_MessageWriteBits(data, bitCount);
}

void MessageWriteBool(const bool value)
{
    s_MessageWriteBool(value);
}

void MessageWriteByte(const int value)
{
    s_MessageWriteByte(value);
}

void MessageWriteChar(const int value)
{
    s_MessageWriteChar(value);
}

void MessageWriteCoord(const float value)
{
    s_MessageWriteCoord(value);
}

void MessageWriteEHandle(CBaseEntity* entity)
{
    s_MessageWriteEHandle(entity);
}

void MessageWriteEntity(const int value)
{
    s_MessageWriteEntity(value);
}

void MessageWriteFloat(const float value)
{
    s_MessageWriteFloat(value);
}

void MessageWriteLong(const std::int32_t value)
{
    s_MessageWriteLong(value);
}

void MessageWriteSBitLong(const std::int32_t value, const int bitCount)
{
    s_MessageWriteSBitLong(value, bitCount);
}

void MessageWriteShort(const int value)
{
    s_MessageWriteShort(value);
}

void MessageWriteString(const char* value)
{
    s_MessageWriteString(value);
}

void MessageWriteUBitLong(const std::uint32_t value, const int bitCount)
{
    s_MessageWriteUBitLong(value, bitCount);
}

void MessageWriteVec3Coord(const Vector3D& value)
{
    s_MessageWriteVec3Coord(value);
}

void MessageWriteVec3Normal(const Vector3D& value)
{
    s_MessageWriteVec3Normal(value);
}

void MessageWriteWord(const int value)
{
    s_MessageWriteWord(value);
}

ON_DLL_LOAD("server.dll", ServerUserMessages, [](CModule module)
{
    s_MessageEnd = module.Offset(0x158880).RCast<MessageEndFn>();
    s_MessageWriteAngle = module.Offset(0x1588B0).RCast<MessageWriteAngleFn>();
    s_MessageWriteAngles = module.Offset(0x158900).RCast<MessageWriteAnglesFn>();
    s_MessageWriteBitVecIntegral = module.Offset(0x158940).RCast<MessageWriteBitVecIntegralFn>();
    s_MessageWriteBits = module.Offset(0x1589B0).RCast<MessageWriteBitsFn>();
    s_MessageWriteBool = module.Offset(0x158A00).RCast<MessageWriteBoolFn>();
    s_MessageWriteByte = module.Offset(0x158A90).RCast<MessageWriteByteFn>();
    s_MessageWriteChar = module.Offset(0x158AD0).RCast<MessageWriteCharFn>();
    s_MessageWriteCoord = module.Offset(0x158B10).RCast<MessageWriteCoordFn>();
    s_MessageWriteEHandle = module.Offset(0x158B60).RCast<MessageWriteEHandleFn>();
    s_MessageWriteEntity = module.Offset(0x158BB0).RCast<MessageWriteEntityFn>();
    s_MessageWriteFloat = module.Offset(0x158BF0).RCast<MessageWriteFloatFn>();
    s_MessageWriteLong = module.Offset(0x158C30).RCast<MessageWriteLongFn>();
    s_MessageWriteSBitLong = module.Offset(0x158C70).RCast<MessageWriteSBitLongFn>();
    s_MessageWriteShort = module.Offset(0x158CC0).RCast<MessageWriteShortFn>();
    s_MessageWriteString = module.Offset(0x158D00).RCast<MessageWriteStringFn>();
    s_MessageWriteUBitLong = module.Offset(0x158D40).RCast<MessageWriteUBitLongFn>();
    s_MessageWriteVec3Coord = module.Offset(0x158DA0).RCast<MessageWriteVec3CoordFn>();
    s_MessageWriteVec3Normal = module.Offset(0x158DE0).RCast<MessageWriteVec3NormalFn>();
    s_MessageWriteWord = module.Offset(0x158E20).RCast<MessageWriteWordFn>();
    s_UserMessageBegin = module.Offset(0x15C520).RCast<UserMessageBeginFn>();
})
