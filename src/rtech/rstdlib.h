#pragma once

#include <cstddef>
#include <cstdint>

struct RFixedArray
{
	uint32_t freeHeadToken;
	uint32_t freeTailIndex;
	uint32_t elementStride;
	uint32_t capacity;
	void* storage;
};

static_assert(sizeof(RFixedArray) == 0x18);
static_assert(offsetof(RFixedArray, storage) == 0x10);


#pragma pack(push, 4)
struct RBitRead
{
	unsigned __int64 m_dataBuf;
	unsigned int m_bitsAvailable;

	RBitRead() : m_dataBuf(0), m_bitsAvailable(64) {};

	FORCEINLINE void ConsumeData(unsigned __int64 input, unsigned int numBits = 64)
	{
		if (numBits > m_bitsAvailable)
		{
			assert(false && "RBitRead::ConsumeData: numBits must be less than or equal to m_bitsAvailable.");
			return;
		}

		m_dataBuf |= input << (64 - numBits);
	}

	FORCEINLINE void ConsumeData(void* input, unsigned int numBits = 64)
	{
		if (numBits > m_bitsAvailable)
		{
			assert(false && "RBitRead::ConsumeData: numBits must be less than or equal to m_bitsAvailable.");
			return;
		}

		m_dataBuf |= *reinterpret_cast<unsigned __int64*>(input) << (64 - numBits);
	}

	FORCEINLINE int BitsAvailable() const { return m_bitsAvailable; };

	FORCEINLINE unsigned __int64 ReadBits(unsigned int numBits)
	{
		assert(numBits <= 64 && "RBitRead::ReadBits: numBits must be less than or equal to 64.");
		return m_dataBuf & ((1ull << numBits) - 1);
	}

	FORCEINLINE void DiscardBits(unsigned int numBits)
	{
		assert(numBits <= 64 && "RBitRead::DiscardBits: numBits must be less than or equal to 64.");
		this->m_dataBuf >>= numBits;
		this->m_bitsAvailable += numBits;
	}
};
#pragma pack(pop)
