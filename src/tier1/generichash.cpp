#include "tier1/generichash.h"

#include <bit>
#include <cstring>

#define HASH_DATA_MIX_CONSTANT_1 0xCC9E2D51u
#define HASH_DATA_MIX_CONSTANT_2 0x1B873593u

void CMurmurHash3::ProcessBlock(std::uint32_t block)
{
    block *= HASH_DATA_MIX_CONSTANT_1;
    block = std::rotl(block, 15);
    block *= HASH_DATA_MIX_CONSTANT_2;

    m_hash ^= block;
    m_hash = std::rotl(m_hash, 13);
    m_hash = m_hash * 5u + 0xE6546B64u;
}

void CMurmurHash3::ProcessByte(const std::uint8_t value)
{
    m_tail |= static_cast<std::uint32_t>(value) << (m_tailBytes * 8u);
    ++m_tailBytes;
    ++m_numBytes;
    if (m_tailBytes == sizeof(m_tail))
    {
        ProcessBlock(m_tail);
        m_tail = 0;
        m_tailBytes = 0;
    }
}

void CMurmurHash3::ProcessBuffer(const void* const data, const unsigned int numBytes)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    unsigned int remaining = numBytes;

    while (m_tailBytes != 0 && remaining != 0)
    {
        ProcessByte(*bytes++);
        --remaining;
    }

    while (remaining >= sizeof(std::uint32_t))
    {
        std::uint32_t block;
        std::memcpy(&block, bytes, sizeof(block));
        if constexpr (std::endian::native == std::endian::big)
            block = std::byteswap(block);
        ProcessBlock(block);
        bytes += sizeof(block);
        remaining -= sizeof(block);
        m_numBytes += sizeof(block);
    }

    while (remaining-- != 0)
        ProcessByte(*bytes++);
}

void CMurmurHash3::ProcessString(const char* text)
{
    do
    {
        ProcessByte(static_cast<std::uint8_t>(*text));
    } while (*text++ != '\0');
}

std::uint32_t CMurmurHash3::Finalize() const
{
    std::uint32_t hash = m_hash;
    if (m_tailBytes != 0)
    {
        std::uint32_t tail = m_tail * HASH_DATA_MIX_CONSTANT_1;
        tail = std::rotl(tail, 15);
        tail *= HASH_DATA_MIX_CONSTANT_2;
        hash ^= tail;
    }

    hash ^= m_numBytes;
    hash ^= hash >> 16;
    hash *= 0x85EBCA6Bu;
    hash ^= hash >> 13;
    hash *= 0xC2B2AE35u;
    hash ^= hash >> 16;
    return hash;
}

unsigned int FASTCALL HashData(const void* const data, const unsigned int numBytes)
{
    CMurmurHash3 hash;
    hash.ProcessBuffer(data, numBytes);
    return hash.Finalize();
}
