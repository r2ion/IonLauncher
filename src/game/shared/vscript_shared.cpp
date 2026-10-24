#include "game/shared/vscript_shared.h"
#include "game/shared/http_request.h"
#include "engine/client/clientstate.h"
#include "engine/server/server.h"
#include "tier1/cvar.h"
#include "tier0/vanilla.h"
#include "game/client/r1/r1_hud_scriptelements.h"
#include "game/server/script_remote_functions.h"

#include <chrono>
#include <string>

#include "vscript/languages/squirrel_re/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqarray.h"
#include "vscript/languages/squirrel_re/squirrel/sqstring.h"
#include "vscript/languages/squirrel_re/squirrel/sqtable.h"

#include "rapidjson/error/en.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#ifdef _MSC_VER
#undef GetObject // fuck microsoft developers
#endif

template <ScriptContext context> void EncodeJSONArray(
	SQArray* arr,
	rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj,
	rapidjson::MemoryPoolAllocator<SourceAllocator>& allocator);

template <ScriptContext context>
void DecodeJsonArray(HSQUIRRELVM sqvm, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* arr)
{
	g_pSquirrel[context]->newarray(sqvm, 0);

	for (auto& itr : arr->GetArray())
	{
		switch (itr.GetType())
		{
		case rapidjson::kObjectType:
			DecodeJsonTable<context>(sqvm, &itr);
			g_pSquirrel[context]->arrayappend(sqvm, -2);
			break;
		case rapidjson::kArrayType:
			DecodeJsonArray<context>(sqvm, &itr);
			g_pSquirrel[context]->arrayappend(sqvm, -2);
			break;
		case rapidjson::kStringType:
			g_pSquirrel[context]->pushstring(sqvm, itr.GetString(), -1);
			g_pSquirrel[context]->arrayappend(sqvm, -2);
			break;
		case rapidjson::kTrueType:
		case rapidjson::kFalseType:
			g_pSquirrel[context]->pushbool(sqvm, itr.GetBool());
			g_pSquirrel[context]->arrayappend(sqvm, -2);
			break;
		case rapidjson::kNumberType:
			if (itr.IsDouble() || itr.IsFloat())
				g_pSquirrel[context]->pushfloat(sqvm, itr.GetFloat());
			else
				g_pSquirrel[context]->pushinteger(sqvm, itr.GetInt());
			g_pSquirrel[context]->arrayappend(sqvm, -2);
			break;
		case rapidjson::kNullType:
			break;
		}
	}
}

template <ScriptContext context>
void DecodeJsonTable(HSQUIRRELVM sqvm, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj)
{
	g_pSquirrel[context]->newtable(sqvm);

	for (auto itr = obj->MemberBegin(); itr != obj->MemberEnd(); itr++)
	{
		switch (itr->value.GetType())
		{
		case rapidjson::kObjectType:
			g_pSquirrel[context]->pushstring(sqvm, itr->name.GetString(), -1);
			DecodeJsonTable<context>(
				sqvm, (rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>*)&itr->value);
			g_pSquirrel[context]->newslot(sqvm, -3, false);
			break;
		case rapidjson::kArrayType:
			g_pSquirrel[context]->pushstring(sqvm, itr->name.GetString(), -1);
			DecodeJsonArray<context>(
				sqvm, (rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>*)&itr->value);
			g_pSquirrel[context]->newslot(sqvm, -3, false);
			break;
		case rapidjson::kStringType:
			g_pSquirrel[context]->pushstring(sqvm, itr->name.GetString(), -1);
			g_pSquirrel[context]->pushstring(sqvm, itr->value.GetString(), -1);

			g_pSquirrel[context]->newslot(sqvm, -3, false);
			break;
		case rapidjson::kTrueType:
		case rapidjson::kFalseType:
			g_pSquirrel[context]->pushstring(sqvm, itr->name.GetString(), -1);
			g_pSquirrel[context]->pushbool(sqvm, itr->value.GetBool());
			g_pSquirrel[context]->newslot(sqvm, -3, false);
			break;
		case rapidjson::kNumberType:
			if (itr->value.IsDouble() || itr->value.IsFloat())
			{
				g_pSquirrel[context]->pushstring(sqvm, itr->name.GetString(), -1);
				g_pSquirrel[context]->pushfloat(sqvm, itr->value.GetFloat());
			}
			else
			{
				g_pSquirrel[context]->pushstring(sqvm, itr->name.GetString(), -1);
				g_pSquirrel[context]->pushinteger(sqvm, itr->value.GetInt());
			}
			g_pSquirrel[context]->newslot(sqvm, -3, false);
			break;
		case rapidjson::kNullType:
			break;
		}
	}
}

template <ScriptContext context> void EncodeJSONTable(
	SQTable* table,
	rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj,
	rapidjson::MemoryPoolAllocator<SourceAllocator>& allocator)
{
	for (int i = 0; i < table->_numOfNodes; i++)
	{
		SQTable::_HashNode* node = &table->_nodes[i];
		if (node->key._Type == OT_STRING)
		{
			rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>> newObj(rapidjson::kObjectType);
			rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>> newArray(rapidjson::kArrayType);

			switch (node->val._Type)
			{
			case OT_STRING:
				obj->AddMember(
					rapidjson::StringRef(node->key._VAL.asString->_val), rapidjson::StringRef(node->val._VAL.asString->_val), allocator);
				break;
			case OT_INTEGER:
				obj->AddMember(rapidjson::StringRef(node->key._VAL.asString->_val), node->val._VAL.asInteger, allocator);
				break;
			case OT_FLOAT:
				obj->AddMember(rapidjson::StringRef(node->key._VAL.asString->_val), node->val._VAL.asFloat, allocator);
				break;
			case OT_BOOL:
				if (node->val._VAL.asInteger)
				{
					obj->AddMember(rapidjson::StringRef(node->key._VAL.asString->_val), true, allocator);
				}
				else
				{
					obj->AddMember(rapidjson::StringRef(node->key._VAL.asString->_val), false, allocator);
				}
				break;
			case OT_TABLE:
				EncodeJSONTable<context>(node->val._VAL.asTable, &newObj, allocator);
				obj->AddMember(rapidjson::StringRef(node->key._VAL.asString->_val), newObj, allocator);
				break;
			case OT_ARRAY:
				EncodeJSONArray<context>(node->val._VAL.asArray, &newArray, allocator);
				obj->AddMember(rapidjson::StringRef(node->key._VAL.asString->_val), newArray, allocator);
				break;
			default:
				spdlog::warn("SQ_EncodeJSON: squirrel type {} not supported", SQTypeNameFromID(node->val._Type));
				break;
			}
		}
	}
}

template <ScriptContext context> void EncodeJSONArray(
	SQArray* arr,
	rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj,
	rapidjson::MemoryPoolAllocator<SourceAllocator>& allocator)
{
	for (int i = 0; i < arr->_usedSlots; i++)
	{
		SQObject* node = &arr->_values[i];

		rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>> newObj(rapidjson::kObjectType);
		rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>> newArray(rapidjson::kArrayType);

		switch (node->_Type)
		{
		case OT_STRING:
			obj->PushBack(rapidjson::StringRef(node->_VAL.asString->_val), allocator);
			break;
		case OT_INTEGER:
			obj->PushBack(node->_VAL.asInteger, allocator);
			break;
		case OT_FLOAT:
			obj->PushBack(node->_VAL.asFloat, allocator);
			break;
		case OT_BOOL:
			if (node->_VAL.asInteger)
				obj->PushBack(rapidjson::StringRef("true"), allocator);
			else
				obj->PushBack(rapidjson::StringRef("false"), allocator);
			break;
		case OT_TABLE:
			EncodeJSONTable<context>(node->_VAL.asTable, &newObj, allocator);
			obj->PushBack(newObj, allocator);
			break;
		case OT_ARRAY:
			EncodeJSONArray<context>(node->_VAL.asArray, &newArray, allocator);
			obj->PushBack(newArray, allocator);
			break;
		default:
			spdlog::info("SQ encode Json type {} not supported", SQTypeNameFromID(node->_Type));
		}
	}
}

template void EncodeJSONTable<ScriptContext::CLIENT>(
	SQTable* table,
	rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj,
	rapidjson::MemoryPoolAllocator<SourceAllocator>& allocator);
template void EncodeJSONTable<ScriptContext::UI>(
	SQTable* table,
	rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj,
	rapidjson::MemoryPoolAllocator<SourceAllocator>& allocator);
template void EncodeJSONTable<ScriptContext::SERVER>(
	SQTable* table,
	rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj,
	rapidjson::MemoryPoolAllocator<SourceAllocator>& allocator);
template void DecodeJsonTable<ScriptContext::CLIENT>(
	HSQUIRRELVM sqvm, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj);
template void DecodeJsonTable<ScriptContext::UI>(
	HSQUIRRELVM sqvm, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj);
template void DecodeJsonTable<ScriptContext::SERVER>(
	HSQUIRRELVM sqvm, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>* obj);


ADD_SQFUNC(
	"table",
	DecodeJSON,
	"string json, bool fatalParseErrors = false",
	"converts a json string to a squirrel table",
	ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	const char* pJson = g_pSquirrel[context]->getstring(sqvm, 1);
	const bool bFatalParseErrors = g_pSquirrel[context]->getbool(sqvm, 2);

	rapidjson_document doc;
	doc.Parse(pJson);
	if (doc.HasParseError())
	{
		g_pSquirrel[context]->newtable(sqvm);

		std::string sErrorString = fmt::format(
			"Failed parsing json file: encountered parse error \"{}\" at offset {}",
			GetParseError_En(doc.GetParseError()),
			doc.GetErrorOffset());

		if (bFatalParseErrors)
		{
			g_pSquirrel[context]->raiseerror(sqvm, sErrorString.c_str());
			return SQRESULT_ERROR;
		}

		spdlog::warn(sErrorString);
		return SQRESULT_NOTNULL;
	}

	DecodeJsonTable<context>(sqvm, (rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<SourceAllocator>>*)&doc);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC(
	"string",
	EncodeJSON,
	"table data",
	"converts a squirrel table to a json string",
	ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	rapidjson_document doc;
	doc.SetObject();

	// temp until this is just the func parameter type
	HSQUIRRELVM vm = (HSQUIRRELVM)sqvm;
	SQTable* table = vm->_stackOfCurrentFunction[1]._VAL.asTable;
	EncodeJSONTable<context>(table, &doc, doc.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	const char* pJsonString = buffer.GetString();

	g_pSquirrel[context]->pushstring(sqvm, pJsonString, -1);
	return SQRESULT_NOTNULL;
}

// asset function StringToAsset( string assetName )
ADD_SQFUNC(
	"asset",
	StringToAsset,
	"string assetName",
	"converts a given string to an asset",
	ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	g_pSquirrel[context]->pushasset(sqvm, g_pSquirrel[context]->getstring(sqvm, 1), -1);
	return SQRESULT_NOTNULL;
}

// string function NSGetLocalPlayerUID()
ADD_SQFUNC(
	"string", NSGetLocalPlayerUID, "", "Returns the local player's uid.", ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	if (g_pLocalPlayerUserID)
	{
		g_pSquirrel[context]->pushstring(sqvm, g_pLocalPlayerUserID);
		return SQRESULT_NOTNULL;
	}

	return SQRESULT_NULL;
}

ADD_SQFUNC(
	"string",
	GetConVarDefaultString,
	"string convarName",
	"Returns convar's default value as a string",
	ScriptContext::UI | ScriptContext::CLIENT | ScriptContext::SERVER)
{
	const char* convarName = g_pSquirrel[context]->getstring(sqvm, 1);
	if (!convarName)
	{
		g_pSquirrel[context]->raiseerror(sqvm, "ConVar name is null");
		return SQRESULT_ERROR;
	}

	auto convar = g_pCVar->FindVar(convarName);
	if (!convar)
	{
		g_pSquirrel[context]->raiseerror(sqvm, fmt::format("ConVar '{}' not found", convarName).c_str());
		return SQRESULT_ERROR;
	}

	const char* defaultValue = convar->m_pszDefaultValue;

	g_pSquirrel[context]->pushstring(sqvm, defaultValue);

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSIsListenServer, "", "Returns true if the current context is a listen server.", ScriptContext::UI | ScriptContext::CLIENT)
{
	if (g_pServer && g_pServer->GetState() > ss_dead)
	{
		g_pSquirrel[context]->pushbool(sqvm, true);
		return SQRESULT_NOTNULL;
	}

	g_pSquirrel[context]->pushbool(sqvm, false);
	return SQRESULT_NOTNULL;
}

template <ScriptContext context> SQRESULT CallOriginalRemoteScriptFunction(const char* functionName, HSQUIRRELVM sqvm)
{
    SquirrelManager* manager = g_pSquirrel[context];
    const auto original = manager->m_funcOriginals.find(functionName);
    if (original != manager->m_funcOriginals.end() && original->second)
        return original->second(sqvm);

    const std::string error = fmt::format("function '{}' is unavailable.", functionName);
    manager->raiseerror(sqvm, error.c_str());
    return SQRESULT_ERROR;
}

REPLACE_SQFUNC(Remote_BeginRegisteringFunctions, ScriptContext::CLIENT | ScriptContext::SERVER)
{
    if constexpr (context == ScriptContext::CLIENT)
        CHudScriptElements::BeginRemoteFunctionRegistration();
    else
        g_ServerScriptRemoteFunctions.BeginRegistration();

    return CallOriginalRemoteScriptFunction<context>("Remote_BeginRegisteringFunctions", sqvm);
}

REPLACE_SQFUNC(Remote_EndRegisteringFunctions, ScriptContext::CLIENT | ScriptContext::SERVER)
{
    const SQRESULT result = CallOriginalRemoteScriptFunction<context>("Remote_EndRegisteringFunctions", sqvm);

    if constexpr (context == ScriptContext::CLIENT)
        CHudScriptElements::FinishRemoteFunctionRegistration();
    else
        g_ServerScriptRemoteFunctions.FinishRegistration();

    return result;
}

template <ScriptContext context> static SQRESULT RegisterRemoteFunction(HSQUIRRELVM sqvm, const bool extended)
{
    bool callOriginal = false;
    SQRESULT result;
    if constexpr (context == ScriptContext::CLIENT)
        result = CHudScriptElements::RegisterRemoteFunction(sqvm, extended, callOriginal);
    else
        result = g_ServerScriptRemoteFunctions.RegisterFunction(sqvm, extended, callOriginal);

    if (SQ_FAILED(result) || !callOriginal)
        return result;

    return CallOriginalRemoteScriptFunction<context>("Remote_RegisterFunction", sqvm);
}

REPLACE_SQFUNC(Remote_RegisterFunction, ScriptContext::CLIENT | ScriptContext::SERVER)
{
    return RegisterRemoteFunction<context>(sqvm, false);
}

ADD_SQFUNC("void", Remote_RegisterFunctionExtended, "string name",
           "Registers an extended client only remote function.",
           ScriptContext::CLIENT | ScriptContext::SERVER)
{
    return RegisterRemoteFunction<context>(sqvm, true);
}

ADD_SQFUNC("string", GetUnixTimestampMilis, "", "Get unix timestamp in milliseconds as a string", ScriptContext::CLIENT | ScriptContext::SERVER | ScriptContext::UI)
{
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = now_ms.time_since_epoch();
    auto value = std::to_string(epoch.count());
    g_pSquirrel[context]->pushstring(sqvm, value.c_str(), -1);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC(
	"int",
	NS_InternalMakeHttpRequest,
	"int method, string baseUrl, table<string, array<string> > headers, table<string, array<string> > queryParams, string contentType, "
	"string body, "
	"int timeout, string userAgent",
	"[Internal use only] Passes the HttpRequest struct fields to be reconstructed in native and used for an http request",
	ScriptContext::CLIENT | ScriptContext::UI | ScriptContext::SERVER)
{
	if (!g_httpRequestHandler || !g_httpRequestHandler->IsRunning())
	{
		spdlog::warn("NS_InternalMakeHttpRequest called while the http request handler isn't running.");
		g_pSquirrel[context]->pushinteger(sqvm, -1);
		return SQRESULT_NOTNULL;
	}

	if (IsHttpDisabled())
	{
		spdlog::warn("NS_InternalMakeHttpRequest called while the game is running with -disablehttprequests."
					 " Please check if requests are allowed using NSIsHttpEnabled() first.");
		g_pSquirrel[context]->pushinteger(sqvm, -1);
		return SQRESULT_NOTNULL;
	}

	HttpRequest request;
	request.method = static_cast<HttpRequestMethod::Type>(g_pSquirrel[context]->getinteger(sqvm, 1));
	request.baseUrl = g_pSquirrel[context]->getstring(sqvm, 2);

	// Read the tables for headers and query parameters.
	SQTable* headerTable = sqvm->_stackOfCurrentFunction[3]._VAL.asTable;
	for (int idx = 0; idx < headerTable->_numOfNodes; ++idx)
	{
		SQTable::_HashNode* node = &headerTable->_nodes[idx];

		if (node->key._Type == OT_STRING && node->val._Type == OT_ARRAY)
		{
			SQArray* valueArray = node->val._VAL.asArray;
			std::vector<std::string> headerValues;

			for (int vIdx = 0; vIdx < valueArray->_usedSlots; ++vIdx)
			{
				if (valueArray->_values[vIdx]._Type == OT_STRING)
				{
					headerValues.push_back(valueArray->_values[vIdx]._VAL.asString->_val);
				}
			}

			request.headers[node->key._VAL.asString->_val] = headerValues;
		}
	}

	SQTable* queryTable = sqvm->_stackOfCurrentFunction[4]._VAL.asTable;
	for (int idx = 0; idx < queryTable->_numOfNodes; ++idx)
	{
		SQTable::_HashNode* node = &queryTable->_nodes[idx];

		if (node->key._Type == OT_STRING && node->val._Type == OT_ARRAY)
		{
			SQArray* valueArray = node->val._VAL.asArray;
			std::vector<std::string> queryValues;

			for (int vIdx = 0; vIdx < valueArray->_usedSlots; ++vIdx)
			{
				if (valueArray->_values[vIdx]._Type == OT_STRING)
				{
					queryValues.push_back(valueArray->_values[vIdx]._VAL.asString->_val);
				}
			}

			request.queryParameters[node->key._VAL.asString->_val] = queryValues;
		}
	}

	request.contentType = g_pSquirrel[context]->getstring(sqvm, 5);
	request.body = g_pSquirrel[context]->getstring(sqvm, 6);
	request.timeout = g_pSquirrel[context]->getinteger(sqvm, 7);
	request.userAgent = g_pSquirrel[context]->getstring(sqvm, 8);

	int handle = g_httpRequestHandler->MakeHttpRequest<context>(request);
	g_pSquirrel[context]->pushinteger(sqvm, handle);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC(
	"bool",
	NSIsHttpEnabled,
	"",
	"Whether or not HTTP requests are enabled. You can opt-out by starting the game with -disablehttprequests.",
	ScriptContext::CLIENT | ScriptContext::UI | ScriptContext::SERVER)
{
	g_pSquirrel[context]->pushbool(sqvm, !IsHttpDisabled());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC(
	"bool",
	NSIsLocalHttpAllowed,
	"",
	"Whether or not HTTP requests can be made to a private network address. You can enable this by starting the game with "
	"-allowlocalhttp.",
	ScriptContext::CLIENT | ScriptContext::UI | ScriptContext::SERVER)
{
	g_pSquirrel[context]->pushbool(sqvm, IsLocalHttpAllowed());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSIsVanilla, "", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pVanillaCompatibility->GetVanillaCompatibility());
	return SQRESULT_NOTNULL;
}
