#include "squirreldocumentation.h"

#include "vscript/vscript.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>

SquirrelDocumentation& SquirrelDocumentation::GetInstance()
{
    static SquirrelDocumentation instance;
    return instance;
}

std::optional<size_t> SquirrelDocumentation::GetContextIndex(ScriptContext context)
{
    const int value = static_cast<int>(context);
    if (value < 0 || value >= static_cast<int>(CONTEXT_COUNT))
        return std::nullopt;
    return static_cast<size_t>(value);
}

std::string SquirrelDocumentation::NormalizeName(const char* name)
{
    std::string normalized = name ? name : "";
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return normalized;
}

std::string SquirrelDocumentation::BuildSignature(const SQFuncRegistration& registration)
{
    const char* returnType = registration.returnTypeString;
    if (!returnType || !*returnType)
        returnType = "var";

    const char* arguments = registration.argTypes ? registration.argTypes : "";
    std::string signature;
    signature.reserve(std::strlen(returnType) + std::strlen(registration.squirrelFuncName) + std::strlen(arguments) + 4);
    signature.append(returnType).push_back(' ');
    signature.append(registration.squirrelFuncName).push_back('(');
    signature.append(arguments).push_back(')');
    return signature;
}

void SquirrelDocumentation::BeginVM(ScriptContext context)
{
    const std::optional<size_t> index = GetContextIndex(context);
    if (!index)
        return;

    std::scoped_lock lock(m_Mutex);
    m_Functions[*index].clear();
}

void SquirrelDocumentation::RegisterFunction(ScriptContext context, const SQFuncRegistration& registration)
{
    const std::optional<size_t> index = GetContextIndex(context);
    if (!index || !registration.squirrelFuncName || !*registration.squirrelFuncName)
        return;

    Function function{
        registration.squirrelFuncName,
        BuildSignature(registration),
        registration.helpText ? registration.helpText : "",
    };

    std::scoped_lock lock(m_Mutex);
    m_Functions[*index].insert_or_assign(NormalizeName(registration.squirrelFuncName), std::move(function));
}

std::vector<SquirrelDocumentation::Function> SquirrelDocumentation::GetFunctions(ScriptContext context) const
{
    const std::optional<size_t> index = GetContextIndex(context);
    if (!index)
        return {};

    std::vector<Function> functions;
    {
        std::scoped_lock lock(m_Mutex);
        functions.reserve(m_Functions[*index].size());
        for (const auto& entry : m_Functions[*index])
            functions.push_back(entry.second);
    }

    std::ranges::sort(functions, {}, &Function::name);
    return functions;
}
