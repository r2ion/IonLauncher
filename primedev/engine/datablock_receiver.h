#pragma once

class NetDatablockReceiver
{
public:
	virtual ~NetDatablockReceiver();
	void *m_pClientState; //0x0008
	bool m_bStartedRecv; //0x0012
	bool m_bCompletedRecv; //0x0013
	uint8_t pad; //0x0014
	int16_t m_TransferId; //0x0015
	int16_t m_nTransferNr; //0x0017
	bool m_bInitialized; //0x0019
	int32_t m_nTransferSize; //0x001A
	int32_t m_nTotalBlocks; //0x001E
	int32_t m_nBlockAckTick; //0x0022
	char pad_0026[4]; //0x0026
	double m_flStartTime; //0x002A
	void *m_BlockStatus; //0x0032
	void *m_pScratchBuffer; //0x003A
};

class NetDatablockSender
{

};
