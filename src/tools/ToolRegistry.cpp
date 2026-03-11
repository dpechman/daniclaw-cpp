#include "ToolRegistry.h"
#include "../utils/Logger.h"
#include <sstream>

void ToolRegistry::registerTool(std::shared_ptr<BaseTool> tool) {
    log().debug("[ToolRegistry] Ferramenta registrada: {}", tool->name());
    tools_[tool->name()] = std::move(tool);
}

BaseTool* ToolRegistry::get(const std::string& name) {
    auto it = tools_.find(name);
    return (it != tools_.end()) ? it->second.get() : nullptr;
}

std::vector<std::shared_ptr<BaseTool>> ToolRegistry::getAll() const {
    std::vector<std::shared_ptr<BaseTool>> result;
    result.reserve(tools_.size());
    for (const auto& [_, tool] : tools_) result.push_back(tool);
    return result;
}

std::string ToolRegistry::getSystemPromptFragment() const {
    if (tools_.empty()) return "";

    std::ostringstream oss;
    oss << R"(
Você tem acesso às seguintes ferramentas (tools). Para usá-las, responda EXCLUSIVAMENTE no JSON:
{"tool_call": {"name": "<nome>", "arguments": {<args>}}}

Ferramentas disponíveis:
)";
    for (const auto& [_, tool] : tools_) {
        oss << tool->toSchemaJson() << "\n";
    }
    oss << "\nSe não precisar de ferramentas, responda em linguagem natural.";
    return oss.str();
}
