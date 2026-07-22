#include "rtech/rui/dynamic_imageatlas.h"
#include "vscript/squirrel/squirrel.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
bool ParseTextureGuid(const char* text, uint64_t& textureGuid)
{
	if (!text)
		return false;

	std::string_view value(text);
	if (value.starts_with("0x") || value.starts_with("0X"))
		value.remove_prefix(2);
	if (value.empty() || value.size() > 16)
		return false;

	const auto result = std::from_chars(value.data(), value.data() + value.size(), textureGuid, 16);
	return result.ec == std::errc() && result.ptr == value.data() + value.size();
}
}

ADD_SQFUNC(
	"int",
	NS_CreateImageAtlas,
	"string textureGUID, string jsonData",
	"Creates a runtime RUI image atlas for a loaded TXTR GUID and returns its handle.",
	ScriptContext::UI | ScriptContext::CLIENT)
{
	const char* textureGuidText = g_pSquirrel[context]->getstring(sqvm, 1);
	const char* jsonData = g_pSquirrel[context]->getstring(sqvm, 2);
	uint64_t textureGuid = 0;
	if (!ParseTextureGuid(textureGuidText, textureGuid))
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			"NS_CreateImageAtlas expected textureGUID to contain 1-16 hexadecimal digits, optionally prefixed with 0x");
		return SQRESULT_ERROR;
	}

	std::string errorMessage;
	const std::optional<int32_t> atlasHandle =
		RuiDynamicImageAtlas_Create(textureGuid, jsonData, errorMessage);
	if (!atlasHandle)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format("NS_CreateImageAtlas: {}", errorMessage).c_str());
		return SQRESULT_ERROR;
	}

	g_pSquirrel[context]->pushinteger(sqvm, *atlasHandle);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC(
	"bool",
	NS_DestroyImageAtlas,
	"int atlasHandle",
	"Destroys a runtime RUI image atlas created by NS_CreateImageAtlas.",
	ScriptContext::UI | ScriptContext::CLIENT)
{
	const int32_t atlasHandle = g_pSquirrel[context]->getinteger(sqvm, 1);
	g_pSquirrel[context]->pushbool(sqvm, RuiDynamicImageAtlas_Destroy(atlasHandle));
	return SQRESULT_NOTNULL;
}
