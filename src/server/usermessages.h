#pragma once

#include <cstdint>

#include "inetmessage.h"

class CBaseEntity;
class IRecipientFilter;
class QAngle;
class Vector3D;

void UserMessageBegin(IRecipientFilter& recipients, const char* messageName, MessageReplayInteraction replayInteraction = NOT_IN_REPLAY);
void MessageEnd();

void MessageWriteAngle(float value);
void MessageWriteAngles(const QAngle& value);
void MessageWriteBitVecIntegral(const Vector3D& value);
void MessageWriteBits(const void* data, int bitCount);
void MessageWriteBool(bool value);
void MessageWriteByte(int value);
void MessageWriteChar(int value);
void MessageWriteCoord(float value);
void MessageWriteEHandle(CBaseEntity* entity);
void MessageWriteEntity(int value);
void MessageWriteFloat(float value);
void MessageWriteLong(std::int32_t value);
void MessageWriteSBitLong(std::int32_t value, int bitCount);
void MessageWriteShort(int value);
void MessageWriteString(const char* value);
void MessageWriteUBitLong(std::uint32_t value, int bitCount);
void MessageWriteVec3Coord(const Vector3D& value);
void MessageWriteVec3Normal(const Vector3D& value);
void MessageWriteWord(int value);
