#pragma once
#include "localize/ilocalize.h"
#include <string>

extern ILocalize* g_pVguiLocalize;

std::string Localize(std::string key, ...);
