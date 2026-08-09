#pragma once

#include "materialsystem/ishader.h"

#include <cstddef>
#include <cstdint>

class CShaderGlue : public IShaderDraw
{
public:
	const char* GetName() const override;
	std::uint64_t Unknown1() override;
	void* Unknown2() override;
	std::uint64_t Unknown3() override;
	int SetupShader(
		std::uint64_t count, std::uint64_t unknown, void* materialData) override;

	const char* name;
	std::uint64_t unknown10;
	std::uint16_t resourceBindingSlot;
	std::uint16_t textureInputCount;
	std::uint16_t shadowSamplerCount;
	std::uint16_t unknownBindingSlot;
	std::uint16_t unknownBindingCount;
	std::uint8_t unknown22[6];
	std::uint64_t unknown28[4];
	void* vertexShader;
	void* pixelShader;
};
static_assert(sizeof(CShaderGlue) == 0x58);
static_assert(offsetof(CShaderGlue, name) == 0x8);
static_assert(offsetof(CShaderGlue, resourceBindingSlot) == 0x18);
static_assert(offsetof(CShaderGlue, textureInputCount) == 0x1A);
static_assert(offsetof(CShaderGlue, shadowSamplerCount) == 0x1C);
static_assert(offsetof(CShaderGlue, unknownBindingSlot) == 0x1E);
static_assert(offsetof(CShaderGlue, unknownBindingCount) == 0x20);
static_assert(offsetof(CShaderGlue, unknown28) == 0x28);
static_assert(offsetof(CShaderGlue, vertexShader) == 0x48);
static_assert(offsetof(CShaderGlue, pixelShader) == 0x50);
