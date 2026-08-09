#pragma once

#include <cstdint>

class IShader
{
public:
	virtual const char* GetName() const = 0; // 0
};
static_assert(sizeof(IShader) == sizeof(void*));

class IShaderDraw : public IShader
{
public:
	virtual std::uint64_t Unknown1() = 0; // 1
	virtual void* Unknown2() = 0; // 2
	virtual std::uint64_t Unknown3() = 0; // 3
	virtual int SetupShader(
		std::uint64_t count, std::uint64_t unknown, void* materialData) = 0; // 4
};
static_assert(sizeof(IShaderDraw) == sizeof(void*));
