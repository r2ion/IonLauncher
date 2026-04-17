#pragma once
#include "inputsystem/InputEnums.h"
#include "util/utils.h"
using CInputSystem__PostEvent = void(__fastcall*)(void* thisObject, InputEventType_t nType, int nTick, int nData, int nData2, int nData3);

static void (*v_CInputSystem__PostEvent)(void* thisObject, InputEventType_t nType, int nTick, int nData, int nData2, int nData3);

const int CROUCHKICK_FIX_BUFFER_MICROSECONDS = 9000;
inline const int BUTTON_CODE_COUNT = 255;


inline long long wallrunStartedTime = 0;
inline long long jumpHitTime = 0;
inline long long crouchHitTime = 0;
inline long long jumpSentTime = 0;

struct KeyInfo_t
{
	char* m_pKeyBinding;
	unsigned char m_nKeyUpTarget : 3;
	unsigned char m_bKeyDown : 1;
};

struct InputHolder
{
	void* thisObject;
	InputEventType_t nType;
	int nTick;
	int data1;
	int data2;
	int data3;

	bool waitingToSend;
	long long timestamp;

	void Hold(void* thisObject, InputEventType_t nType, int nTick, int data1, int data2, int data3)
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
		v_CInputSystem__PostEvent(thisObject, nType, nTick, data1, data2, data3);
		waitingToSend = false;
	}
};

void FindBinds();

void CFKPostEvent(void* thisObject, InputEventType_t nType, int nTick, int data1, int data2, int data3);

static void EnsureCKFOriginals() {
	if (!v_CInputSystem__PostEvent)
	{
		v_CInputSystem__PostEvent = HookSys::GetOriginalFunction<CInputSystem__PostEvent>(HookSys::FindHook("CInputSystem__PostEvent"));
	}
}
