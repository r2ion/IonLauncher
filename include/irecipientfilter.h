#pragma once

#include <cstddef>

class IRecipientFilter
{
public:
    virtual ~IRecipientFilter() = default; // 0
    virtual bool IsReliable() const = 0; // 1
    virtual void MakeReliable() = 0; // 2
    virtual bool IsInitMessage() const = 0; // 3
    virtual int GetRecipientCount() const = 0; // 4
    virtual int GetRecipientIndex(int slot) const = 0; // 5
    virtual bool IsReplayMessage(int slot) const = 0; // 6
};

static_assert(sizeof(IRecipientFilter) == sizeof(void*));
