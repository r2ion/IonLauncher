#include "rtech/rui/dynamic_imageatlas.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

ADD_SQFUNC(
	"int",
	NS_CreateImageAtlas,
	"string textureGUID, string jsonData",
	"Creates a runtime RUI image atlas for a loaded TXTR GUID. Keep the TXTR loaded until the handle is destroyed.",
	ScriptContext::UI | ScriptContext::CLIENT)
{
	const char* textureGuidText = g_pSquirrel[context]->getstring(sqvm, 1);
	const char* jsonData = g_pSquirrel[context]->getstring(sqvm, 2);
	std::string_view textureGuidValue(textureGuidText ? textureGuidText : "");
	if (textureGuidValue.starts_with("0x") || textureGuidValue.starts_with("0X"))
		textureGuidValue.remove_prefix(2);

	uint64_t textureGuid = 0;
	const auto parseResult = std::from_chars(
		textureGuidValue.data(),
		textureGuidValue.data() + textureGuidValue.size(),
		textureGuid,
		16);
	if (textureGuidValue.empty() || textureGuidValue.size() > 16
		|| parseResult.ec != std::errc() || parseResult.ptr != textureGuidValue.data() + textureGuidValue.size())
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			"NS_CreateImageAtlas expected textureGUID to contain 1-16 hexadecimal digits, optionally prefixed with 0x");
		return SQRESULT_ERROR;
	}

	std::string errorMessage;
	const std::optional<CDynamicImageAtlas::Handle> atlasHandle =
		CDynamicImageAtlas::Create(textureGuid, jsonData, errorMessage);
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
	g_pSquirrel[context]->pushbool(sqvm, CDynamicImageAtlas::Destroy(atlasHandle));
	return SQRESULT_NOTNULL;
}
