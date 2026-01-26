#include <string>
#include <vector>
#include "localize.h"

static bool (*EngineClient__Localize)(
	const char* key, const char** args, unsigned int num_args, char* output_buffer, unsigned int outputSize, unsigned int* a6) = nullptr;


ADD_SQFUNC("string", Localize, "string format,...", "Localize string", ScriptContext::SERVER) {

	auto key = g_pSquirrel[context]->getstring(sqvm,1);
	auto num_args = (sqvm->_top - sqvm->_stackbase) - 2;
	if (num_args > 10)
	{
		g_pSquirrel[context]->raiseerror(sqvm, "Too many arguments");
		return SQRESULT_ERROR;
	}
	std::vector<const char*> arg_list;
	for (int i = 1; i < num_args + 1; i++)
	{
		arg_list.push_back(g_pSquirrel[context]->getstring(sqvm, i + 1));
	}
	char output_buffer[2048];
	if (!EngineClient__Localize(key, arg_list.data(), arg_list.size(), output_buffer, sizeof(output_buffer), 0))
	{
		g_pSquirrel[context]->raiseerror(sqvm, "String too large for internal buffer.");
		return SQRESULT_ERROR;
	}
	g_pSquirrel[context]->pushstring(sqvm, output_buffer);
	return SQRESULT_NOTNULL;
}

std::string Localize(std::string key, ...) {
	va_list args;
	va_start(args, key);

	std::vector<const char*> arg_list;
	auto arg = va_arg(args, const char*);
	while (arg)
	{
		arg_list.push_back(arg);
		arg = va_arg(args, const char*);
	}
	va_end(args);

	char output_buffer[4096];

	EngineClient__Localize(key.c_str(), arg_list.data(), arg_list.size(), output_buffer, sizeof(output_buffer), 0);

	return std::string(output_buffer);

}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", LocalizeEngine, ConCommand, [](CModule module)
{
	EngineClient__Localize = module.Offset(0xF8CA0).RCast<decltype(EngineClient__Localize)>();
})
