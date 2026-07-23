#pragma once

#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class ScriptContext : int;
struct SQFuncRegistration;

class SquirrelDocumentation final
{
  public:
    struct Function
    {
        std::string name;
        std::string signature;
        std::string description;
    };

    static SquirrelDocumentation& GetInstance();

    void BeginVM(ScriptContext context);
    void RegisterFunction(ScriptContext context, const SQFuncRegistration& registration);
    std::vector<Function> GetFunctions(ScriptContext context) const;

  private:
    static constexpr size_t CONTEXT_COUNT = 3;

    SquirrelDocumentation() = default;

    static std::optional<size_t> GetContextIndex(ScriptContext context);
    static std::string NormalizeName(const char* name);
    static std::string BuildSignature(const SQFuncRegistration& registration);

    mutable std::mutex m_Mutex;
    std::array<std::unordered_map<std::string, Function>, CONTEXT_COUNT> m_Functions;
};
