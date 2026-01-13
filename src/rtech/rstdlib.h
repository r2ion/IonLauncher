#pragma once

class RFixedArray
{
	// have not these int fields in r2, but the size is accurate at least
	int index;
	int slotsLeft;
	int structSize;
	int modMask;
	void* buffer;
};
