#pragma once

#include "pakstate.h"

const char* Pak_StatusToString(const PakStatus_e status);
PakGuid_t Pak_StringToGuid(const char* string);
