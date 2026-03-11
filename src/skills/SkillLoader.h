#pragma once
#include "../providers/ILlmProvider.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>

struct SkillMeta {
    std::string name;
    std::string description;
    std::string content;    // Conteúdo completo do SKILL.md
    std::string dirPath;
};

/**
 * Carrega skills da pasta .agents/skills (hot-reload a cada request).
 */
class SkillLoader {
public:
    SkillLoader();
    std::vector<SkillMeta> loadAll();

private:
    std::string skillsDir_;
};

/**
 * Router que usa LLM para detectar qual skill usar.
 */
class SkillRouter {
public:
    SkillRouter();
    std::optional<SkillMeta> route(const std::string& userInput,
                                    const std::vector<SkillMeta>& skills);
private:
    std::shared_ptr<ILlmProvider> provider_;
};
