#pragma once

#include "engine/bitbuf.h"

class CModRemoteFunction
{
private:
	std::vector<std::pair<char, std::string>> m_typedParams;
	std::string m_pszIdentifier;

public:
	~CModRemoteFunction() = default;

	virtual void Init();
	virtual void MsgFunc_CallFunction(bf_read* msg) = 0;
	virtual void MsgFunc_Checksum(bf_read* msg) = 0;
};
