#pragma once
#include <string>
#include <map>

struct ToolResult {
    std::string output;
    std::string error;
    bool hasError() const { return !error.empty(); }
};

/**
 * Classe base (abstrata) para todas as ferramentas do agente.
 */
class BaseTool {
public:
    virtual ~BaseTool() = default;
    virtual const std::string& name() const = 0;
    virtual const std::string& description() const = 0;
    virtual ToolResult execute(const std::map<std::string, std::string>& args) = 0;

    /// Serializa o schema da tool para injeção no system prompt
    virtual std::string toSchemaJson() const;
};
