#pragma once

#include <cstddef>
#include <cstdint>

class CClient;
class CClientState;

inline constexpr std::size_t DataBlockFragmentSize = 1024;
inline constexpr std::size_t DataBlockMaxFragments = 256;
inline constexpr std::size_t DataBlockDebugNameLength = 64;
inline constexpr std::size_t DataBlockScratchBufferSize = DataBlockFragmentSize * DataBlockMaxFragments;

class NetDataBlockSender
{
public:
	virtual ~NetDataBlockSender() = default;
	virtual void SendDataBlock(std::uint8_t transferId, std::uint32_t transferSize,
		std::uint8_t transferNumber, std::uint8_t blockNumber,
		const std::uint8_t* blockData, std::uint32_t blockSize) = 0;

protected:
	CClient* m_pReceiver;
	bool m_bInitialized;
	bool m_bResendThrottled;
	bool m_bMultiplayer;
	std::uint8_t m_nTransferId;
	std::uint8_t m_nTransferNumber;
	std::uint8_t m_Padding15[3];
	std::int32_t m_nTransferSize;
	std::int32_t m_nTotalBlocks;
	std::int32_t m_nAcknowledgedBlocks;
	std::uint32_t m_Reserved24;
	double m_flSendBudget;
	double m_flCurrentSendTime;
	double m_flFirstSendTime;
	double m_flLastAcknowledgedTime;
	double* m_pBlockSendTimes;
	char m_szDebugName[DataBlockDebugNameLength];
	std::uint8_t* m_pBlockAckStatus;
	std::uint8_t* m_pScratchBuffer;
};

#pragma pack(push, 1)
class NetDataBlockReceiver
{
public:
	virtual ~NetDataBlockReceiver();
	virtual void AcknowledgeTransmission(std::uint8_t transferId, std::uint8_t transferNumber,
		std::uint8_t blockNumber) = 0;

	CClientState* m_pClientState; // 0x0008
	bool m_bStartedRecv; // 0x0010
	bool m_bCompletedRecv; // 0x0011
	std::uint8_t m_receiveState; // 0x0012
	std::int16_t m_nTransferId; // 0x0013
	std::int16_t m_nTransferNumber; // 0x0015
	bool m_bInitialized; // 0x0017
	std::int32_t m_nTransferSize; // 0x0018
	std::int32_t m_nTotalBlocks; // 0x001C
	std::int32_t m_nBlockAckTick; // 0x0020
	std::uint8_t m_Padding0024[4]; // 0x0024
	double m_flStartTime; // 0x0028
	std::uint8_t* m_pBlockStatus; // 0x0030
	std::uint8_t* m_pScratchBuffer; // 0x0038
};
#pragma pack(pop)

static_assert(sizeof(NetDataBlockSender) == 0xA0);
static_assert(sizeof(NetDataBlockReceiver) == 0x40);
