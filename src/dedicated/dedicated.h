#pragma once
#include "appframework/IAppSystem.h"

class CDedicatedExports : public IAppSystem
{
  public:
    virtual ~CDedicatedExports() = default;                                        // 0
    bool Connect(CreateInterfaceFn factory) override;                              // 1
    void Disconnect() override;                                                    // 2
    void* QueryInterface(const char* interfaceName) override;                      // 3
    InitReturnVal_t Init() override;                                               // 4
    void Shutdown() override;                                                      // 5
    const AppSystemInfo_t* GetDependencies() override;                             // 6
    void Reconnect(CreateInterfaceFn factory, const char* interfaceName) override; // 7
    virtual void Sys_Printf(const char* msg);                                      // 8
    virtual void RunServer();                                                      // 9
};

static_assert(sizeof(CDedicatedExports) == 8);

bool IsDedicatedServer();
