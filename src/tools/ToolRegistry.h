#pragma once
#include "BaseTool.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

/**
 * Registry dinâmico de ferramentas.
 */
class ToolRegistry {
public:
    void registerTool(std::shared_ptr<BaseTool> tool);
    BaseTool* get(const std::string& name);
    std::vector<std::shared_ptr<BaseTool>> getAll() const;
    std::string getSystemPromptFragment() const;

private:
    std::unordered_map<std::string, std::shared_ptr<BaseTool>> tools_;
};
