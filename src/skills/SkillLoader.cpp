#include "SkillLoader.h"
#include "../config/Config.h"
#include "../utils/Logger.h"
#include "../providers/ProviderFactory.h"
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ── SkillLoader ───────────────────────────────────────────────────────────────

SkillLoader::SkillLoader() {
    skillsDir_ = cfg().get("SKILLS_DIR", "./.agents/skills");
}

std::vector<SkillMeta> SkillLoader::loadAll() {
    std::vector<SkillMeta> skills;

    if (!fs::exists(skillsDir_)) {
        log().warn("[SkillLoader] Pasta de skills não encontrada: {}", skillsDir_);
        return skills;
    }

    for (const auto& entry : fs::directory_iterator(skillsDir_)) {
        if (!entry.is_directory()) continue;

        fs::path skillMdPath = entry.path() / "SKILL.md";
        if (!fs::exists(skillMdPath)) continue;

        try {
            std::ifstream f(skillMdPath);
            if (!f) continue;

            std::ostringstream ss;
            ss << f.rdbuf();
            std::string raw = ss.str();

            // Extrai frontmatter YAML (--- ... ---)
            SkillMeta meta;
            meta.dirPath = entry.path().string();

            std::regex fmRe(R"(^---\r?\n([\s\S]*?)\r?\n---\r?\n([\s\S]*)$)");
            std::smatch match;
            if (std::regex_search(raw, match, fmRe)) {
                try {
                    YAML::Node node = YAML::Load(match[1].str());
                    meta.name        = node["name"]        ? node["name"].as<std::string>()        : entry.path().filename().string();
                    meta.description = node["description"] ? node["description"].as<std::string>() : "";
                } catch (...) {
                    meta.name = entry.path().filename().string();
                }
                meta.content = match[2].str();
            } else {
                meta.name    = entry.path().filename().string();
                meta.content = raw;
            }

            skills.push_back(std::move(meta));
        } catch (const std::exception& e) {
            log().error("[SkillLoader] Erro ao carregar '{}': {}", entry.path().string(), e.what());
        }
    }

    std::string names;
    for (const auto& s : skills) names += s.name + " ";
    log().info("[SkillLoader] {} skill(s) carregada(s): {}", skills.size(), names);
    return skills;
}

// ── SkillRouter ───────────────────────────────────────────────────────────────

SkillRouter::SkillRouter() {
    std::string routerProvider = cfg().get("SKILL_ROUTER_PROVIDER",
                                            cfg().get("LLM_PROVIDER", "groq"));
    provider_ = ProviderFactory::create(routerProvider);
}

std::optional<SkillMeta> SkillRouter::route(const std::string& userInput,
                                              const std::vector<SkillMeta>& skills) {
    if (skills.empty()) return std::nullopt;

    std::string skillList;
    for (const auto& s : skills) {
        skillList += "- \"" + s.name + "\": " + s.description + "\n";
    }

    std::string prompt =
        "Você é um roteador de intenções. Responda APENAS o JSON:\n"
        "{\"skillName\": \"<nome_exato>\"} ou {\"skillName\": null}\n\n"
        "Skills disponíveis:\n" + skillList +
        "\nMensagem do usuário: \"" + userInput + "\"\n\n"
        "Responda SOMENTE o JSON, sem explicações.";

    try {
        std::string response = provider_->chat({{"user", prompt}});

        std::regex jsonRe(R"(\{[\s\S]*"skillName"[\s\S]*\})");
        std::smatch match;
        if (!std::regex_search(response, match, jsonRe)) return std::nullopt;

        auto j = json::parse(match[0].str());
        if (j["skillName"].is_null()) return std::nullopt;

        std::string skillName = j["skillName"].get<std::string>();
        for (const auto& s : skills) {
            if (s.name == skillName) {
                log().info("[SkillRouter] ✅ Skill selecionada: {}", s.name);
                return s;
            }
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        log().error("[SkillRouter] Erro no roteamento: {}", e.what());
        return std::nullopt;
    }
}
