#pragma once

#include "engine/shared/datablock.h"

class ClientDataBlockReceiver : public NetDataBlockReceiver
{
public:
	~ClientDataBlockReceiver() override;
	void AcknowledgeTransmission(std::uint8_t transferId, std::uint8_t transferNumber,
		std::uint8_t blockNumber) override;
};

static_assert(sizeof(ClientDataBlockReceiver) == 0x40);
