#pragma once

#include "engine/shared/datablock.h"

#include <cstddef>
#include <cstdint>

class ServerDataBlockSender final : public NetDataBlockSender
{
  public:
    void SendDataBlock(std::uint8_t transferId, std::uint32_t transferSize,
                       std::uint8_t transferNumber, std::uint8_t blockNumber,
                       const std::uint8_t* blockData, std::uint32_t blockSize) override;
};

struct ServerDataBlock
{
    std::int32_t m_nLastBlockTick;
    std::int32_t m_nBlockSize;
    ServerDataBlockSender m_Sender;
};

struct ServerDataBlockHeader_s
{
    bool m_bCompressed;
};

static_assert(sizeof(ServerDataBlockSender) == 0xA0);
static_assert(sizeof(ServerDataBlock) == 0xA8);
static_assert(offsetof(ServerDataBlock, m_nLastBlockTick) == 0x0);
static_assert(offsetof(ServerDataBlock, m_nBlockSize) == 0x4);
static_assert(offsetof(ServerDataBlock, m_Sender) == 0x8);
static_assert(sizeof(ServerDataBlockHeader_s) == 0x1);
