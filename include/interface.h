#pragma once

#define CREATEINTERFACE_PROCNAME "CreateInterface"

class IBaseInterface
{
};

enum class InterfaceStatus : int
{
    IFACE_OK = 0,
    IFACE_FAILED,
};

using CreateInterfaceFn = void* (*)(const char* pName, int* pReturnCode);
using InstantiateInterfaceFn = void* (*)();
