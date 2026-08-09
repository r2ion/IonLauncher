#pragma once

inline constexpr char SHADER_SYSTEM_INTERFACE_VERSION[] = "ShaderSystem002";

class IShaderSystem
{
public:
	virtual bool IsUsingGraphics() const = 0; // 0
};
static_assert(sizeof(IShaderSystem) == sizeof(void*));
