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
	m_VMFunctions[*index].clear();
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
	m_VMFunctions[*index].insert_or_assign(NormalizeName(registration.squirrelFuncName), std::move(function));
}

void SquirrelDocumentation::RegisterStaticFunctions(ScriptContext context, const SQFuncRegistration* registrations, size_t count)
{
	const std::optional<size_t> index = GetContextIndex(context);
	if (!index || !registrations)
		return;

	std::scoped_lock lock(m_Mutex);
	auto& functions = m_StaticFunctions[*index];
	for (size_t registrationIndex = 0; registrationIndex < count; registrationIndex++)
	{
		const SQFuncRegistration& registration = registrations[registrationIndex];
		if (!registration.squirrelFuncName || !*registration.squirrelFuncName)
			continue;

		Function function{
		    registration.squirrelFuncName,
		    BuildSignature(registration),
		    registration.helpText ? registration.helpText : "",
		};
		const std::string normalizedName = NormalizeName(registration.squirrelFuncName);
		const auto existing = functions.find(normalizedName);
		if (existing == functions.end() || (existing->second.description.empty() && !function.description.empty()))
			functions.insert_or_assign(normalizedName, std::move(function));
	}
}

std::vector<SquirrelDocumentation::Function> SquirrelDocumentation::GetFunctions(ScriptContext context) const
{
    const std::optional<size_t> index = GetContextIndex(context);
    if (!index)
        return {};

    std::vector<Function> functions;
    {
        std::scoped_lock lock(m_Mutex);
		const auto& staticFunctions = m_StaticFunctions[*index];
		const auto& vmFunctions = m_VMFunctions[*index];
		functions.reserve(staticFunctions.size() + vmFunctions.size());
        for (const auto& [name, function] : staticFunctions)
		{
			if (!vmFunctions.contains(name))
				functions.push_back(function);
		}
		for (const auto& entry : vmFunctions)
            functions.push_back(entry.second);
    }

    std::ranges::sort(functions, {}, &Function::name);
    return functions;
}
