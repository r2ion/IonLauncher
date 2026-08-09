#pragma once

#include "tier1/convar.h"

void RegisterConCommand(const char* name, FnCommandCallback_t callback, const char* helpString, int flags);
void RegisterConCommand(
	const char* name,
	FnCommandCallback_t callback,
	const char* helpString,
	int flags,
	FnCommandCompletionCallback completionCallback);
