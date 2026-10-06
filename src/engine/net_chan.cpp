#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>

#include "core/tier0.h"
#include "engine/net_chan.h"
#include "tier0/callbacks.h"

DECLARE_MODULE(NetChanHooks)

using CNetChanCanPacket_t = bool (*)(const CNetChan*);
using CNetChanProcessMessages_t = bool (*)(CNetChan*, bf_read*);
using CNetChanSendNetMsg_t = bool (*)(CNetChan*, INetMessage*, bool, bool);
using CNetChanSendDatagram_t = std::int32_t (*)(CNetChan*, bf_write*);
using CNetChanSetChoked_t = void (*)(CNetChan*);

CNetChanCanPacket_t CNetChan__CanPacket;
CNetChanProcessMessages_t CNetChan__ProcessMessages;
CNetChanSendNetMsg_t CNetChan__SendNetMsg;
CNetChanSendDatagram_t CNetChan__SendDatagram;
CNetChanSetChoked_t CNetChan__SetChoked;

bool CNetChan::CanPacket() const
{
	return CNetChan__CanPacket(this);
}

bool CNetChan::SendNetMsg(INetMessage& message, const bool forceReliable, const bool voice)
{
	return CNetChan__SendNetMsg(this, &message, forceReliable, voice);
}

std::int32_t CNetChan::SendDatagram(bf_write* const datagram)
{
	return CNetChan__SendDatagram(this, datagram);
}

void CNetChan::SetChoked()
{
	CNetChan__SetChoked(this);
}

bool CNetChan::ProcessMessages(bf_read* const message)
{
	return CNetChan__ProcessMessages(this, message);
}

void CNetChan::FreeReceiveList()
{
	m_ReceiveList.blockSize = 0;
	m_ReceiveList.transferSize = 0;

	if (m_ReceiveList.buffer)
	{
		g_pMemAllocSingleton->Free(m_ReceiveList.buffer);
		m_ReceiveList.buffer = nullptr;
	}
}

bool CNetChan::ReadSubChannelData(bf_read& buffer)
{
	uint8_t discardBuffer[FRAGMENT_SIZE];

	m_bInReliableState = true;

	buffer.ReadUBitLong(32);
	buffer.ReadUBitLong(32);
	const uint32_t wireFragment = buffer.ReadUBitLong(10);

	if (wireFragment == 0)
	{
		if (buffer.ReadOneBit() != 0)
		{
			const uint32_t remoteNonce = buffer.ReadUBitLong(32);
			if (remoteNonce != m_nNonceRemote)
			{
				m_nNonceRemote = remoteNonce;
				m_bReceivedRemoteNonce = true;
				m_bInReliableState = false;
				m_nSubInFragments = 0;
				FreeReceiveList();
				m_bPendingRemoteNonceAck = true;
			}
		}
		else
		{
			m_bPendingRemoteNonceAck = false;
		}
	}
	else
	{
		m_bPendingRemoteNonceAck = false;
	}

	int fragmentsToSkip = INT_MAX;
	if (m_bReceivedRemoteNonce)
	{
		fragmentsToSkip = (m_nSubInFragments & 0x3FF) - static_cast<int>(wireFragment);
		if (fragmentsToSkip < 0)
			fragmentsToSkip += 0x400;
	}

	for (;;)
	{
		buffer.ReadUBitLong(32);
		const bool firstFragment = buffer.ReadOneBit() != 0;

		int transferSize = -1;
		int fragmentSize;
		bool isCompressed = false;
		int64_t uncompressedSize = -1;

		if (firstFragment)
		{
			if (fragmentsToSkip == 0 && m_ReceiveList.buffer)
				return false;

			transferSize = static_cast<int>(buffer.ReadUBitLong(19));
			if (transferSize > NET_MAX_PAYLOAD)
				return false;

			fragmentSize = (std::min)(transferSize, FRAGMENT_SIZE);
			isCompressed = buffer.ReadOneBit() != 0;
			if (isCompressed)
				uncompressedSize = buffer.ReadUBitLong(22);
		}
		else
		{
			const bool lastFragment = buffer.ReadOneBit() != 0;
			fragmentSize = lastFragment ? static_cast<std::int32_t>(buffer.ReadUBitLong(10)) : FRAGMENT_SIZE;
		}

		if (fragmentSize > FRAGMENT_SIZE)
			return false;

		if (fragmentsToSkip != 0)
		{
			buffer.ReadBytes(discardBuffer, fragmentSize);
			--fragmentsToSkip;
		}
		else
		{
			dataFragments_t& receive = m_ReceiveList;

			if (firstFragment)
			{
				receive.transferSize = transferSize;
				receive.blockSize = transferSize;
				receive.currentOffset = 0;
				receive.buffer = static_cast<char*>(g_pMemAllocSingleton->Alloc(transferSize));
				receive.isCompressed = isCompressed;
				receive.uncompressedSize = uncompressedSize;
			}

			if (receive.currentOffset + fragmentSize > receive.transferSize)
				return false;

			buffer.ReadBytes(receive.buffer + receive.currentOffset, fragmentSize);
			receive.currentOffset += fragmentSize;
			++m_nSubInFragments;

			if (receive.currentOffset == receive.transferSize)
			{
				if (receive.isCompressed)
				{
					size_t outputSize = static_cast<size_t>(receive.uncompressedSize);
					char* output = static_cast<char*>(g_pMemAllocSingleton->Alloc((outputSize + 3) & ~size_t {3}));

					NET_BufferToBufferDecompress(
						output, &outputSize, receive.buffer, static_cast<size_t>(receive.blockSize));
					g_pMemAllocSingleton->Free(receive.buffer);

					receive.buffer = output;
					receive.blockSize = outputSize;
					receive.isCompressed = false;
				}

				bf_read messages(receive.buffer, static_cast<size_t>(receive.blockSize));
				const bool processed = this->ProcessMessages(&messages);

				if (receive.buffer)
				{
					g_pMemAllocSingleton->Free(receive.buffer);
					receive.buffer = nullptr;
				}

				if (!processed)
					return false;
			}
		}

		if (buffer.ReadOneBit() == 0)
			return true;
	}
}

// clang-format off
DECLARE_HOOK(CNetChan::ReadSubChannelData, engine.dll + 0x211EF0,
([](auto& hook, CNetChan* channel, bf_read* buffer) -> bool
// clang-format on
{
	NOTE_UNUSED(hook);
	return channel->ReadSubChannelData(*buffer);
}))

ON_DLL_LOAD("engine.dll", NetChan, [](CModule module)
{
	CNetChan__CanPacket = module.Offset(0x20F620).RCast<CNetChanCanPacket_t>();
	CNetChan__ProcessMessages = module.Offset(0x2140A0).RCast<CNetChanProcessMessages_t>();
	CNetChan__SendNetMsg = module.Offset(0x213270).RCast<CNetChanSendNetMsg_t>();
	CNetChan__SendDatagram = module.Offset(0x212CD0).RCast<CNetChanSendDatagram_t>();
	CNetChan__SetChoked = module.Offset(0x213760).RCast<CNetChanSetChoked_t>();

	DISPATCH_MODULE(NetChanHooks)
})
