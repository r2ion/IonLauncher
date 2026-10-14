#pragma once

#include "mathlib/vector.h"

enum ShakeCommand_t : int
{
	SHAKE_START = 0,
	SHAKE_STOP,
	SHAKE_AMPLITUDE,
	SHAKE_FREQUENCY,
	SHAKE_START_RUMBLEONLY,
	SHAKE_START_NORUMBLE,
	SHAKE_START_NON_LEGACY,
};

struct ScreenShake_t
{
	ShakeCommand_t command;
	float amplitude;
	float frequency;
	float duration;
	Vector3D direction;
};
