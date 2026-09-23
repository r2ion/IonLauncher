#pragma once

#include <cstddef>

#include "server/variant_t.h"

class CBaseEntity;
class CEventAction;
class IRestore;
class ISave;

class CBaseEntityOutput
{
  public:
    virtual ~CBaseEntityOutput();
    virtual int Save(ISave& save);
    virtual int Restore(IRestore& restore, int elementCount);

    void ParseEventAction(const char* eventData);
    void AddEventAction(CEventAction* eventAction);
    void RemoveEventAction(CEventAction* eventAction);
    void FireOutput(variant_t value, CBaseEntity* activator, CBaseEntity* caller, float delay = 0.0f);

  protected:
    variant_t m_Value;          // 0x08
    CEventAction* m_ActionList; // 0x20
};

class COutputEvent : public CBaseEntityOutput
{
  public:
    void FireOutput(CBaseEntity* activator, CBaseEntity* caller, float delay = 0.0f);
};

static_assert(sizeof(CBaseEntityOutput) == 0x28);
static_assert(sizeof(COutputEvent) == 0x28);
