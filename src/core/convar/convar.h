#pragma once

#include "tier1/convar.h"

#include <span>
#include <string>
#include <utility>

std::span<const std::pair<int, const char*>> GetConVarFlagNames();

int ParseConVarFlagsString(std::string modName, std::string flags);
