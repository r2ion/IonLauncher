#pragma once
#include "engine/keys.h"
#include "inputsystem/inputsystem.h"
#include "util/utils.h"
const int CROUCHKICK_FIX_BUFFER_MICROSECONDS = 9000;
inline const int CROUCHKICK_BUTTON_CODE_COUNT = 255;


inline long long wallrunStartedTime = 0;
inline long long jumpHitTime = 0;
inline long long crouchHitTime = 0;
inline long long jumpSentTime = 0;


struct InputHolder
{
	CInputSystem* thisObject;
	InputEventType_t nType;
	int nTick;
	int data1;
	int data2;
	int data3;

	bool waitingToSend;
	long long timestamp;

	void Hold(CInputSystem* thisObject, InputEventType_t nType, int nTick, int data1, int data2, int data3)
	{
		this->thisObject = thisObject;
		this->nType = nType;
		this->nTick = nTick;
		this->data1 = data1;
		this->data2 = data2;
		this->data3 = data3;
		waitingToSend = true;
	}
	void Release()
	{
		thisObject->PostEvent(nType, nTick, data1, data2, data3);
		waitingToSend = false;
	}
};

void FindBinds();

bool CFKPostEvent(CInputSystem* thisObject, InputEventType_t nType, int nTick, int data1, int data2, int data3);
