#pragma once

#include <cstddef>
#include <cstdint>

enum class ScriptRemoteFunctionWireType : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    Float = 3,
};

#define MAX_SCRIPT_REMOTE_FUNCTION_PARAMETERS 10
#define MAX_NAMED_REMOTE_FUNCTION_NAME 1024
