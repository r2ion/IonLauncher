#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Psapi.h>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <algorithm>
#include <array>
#include <bcrypt.h>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cmath>
#include <comdef.h>
#include <dbghelp.h>
#include <direct.h>
#include <emmintrin.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <intrin.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <objbase.h>
#include <set>
#include <setjmp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <smmintrin.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <timeapi.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>

#include "tier0/memstd.h"

#include "common/pseudodefs.h"
#include "common/sdkdefs.h"
#include "common/x86defs.h"

#include <d3d11.h>

#include "tier0/hooks.h"
#include "tier0/memaddr.h"
#include "tier0/module.h"
#include "tier0/vanilla.h"
