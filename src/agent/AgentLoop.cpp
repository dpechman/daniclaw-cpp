#include "AgentLoop.h"
#include "../utils/Logger.h"
#include "../config/Config.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>

using json = nlohmann::json;

AgentLoop::AgentLoop(std::shared_ptr<ILlmProvider> provider,
                     std::shared_ptr<ToolRegistry>  registry)
    : provider_(std::move(provider))
    , registry_(std::move(registry))
    , maxIterations_(cfg().getInt("MAX_ITERATIONS", 5))
{}

AgentLoopResult AgentLoop::run(
    const std::vector<LlmMessage>& messages,
    const std::string& systemPrompt,
    bool requiresAudio
) {
    std::string toolFragment = registry_->getSystemPromptFragment();
    std::string fullSystem   = systemPrompt;
    if (!toolFragment.empty()) fullSystem += "\n\n" + toolFragment;

    std::vector<LlmMessage> history = messages;

    for (int iter = 1; iter <= maxIterations_; ++iter) {
        log().info("[AgentLoop] ── Iteração {}/{} ──", iter, maxIterations_);

        std::string response;
        try {
            response = provider_->chat(history, fullSystem);
        } catch (const std::exception& e) {
            log().error("[AgentLoop] Erro na chamada ao LLM: {}", e.what());
            return {
                "⚠️ Erro ao contatar o modelo de IA (" +
                provider_->name() + " · " +
                cfg().get(provider_->name() + "_MODEL", "desconhecido") +
                "). Verifique a chave de API.",
                requiresAudio
            };
        }

        log().debug("[AgentLoop] Response ({}chars): {}...",
            response.size(), response.substr(0, std::min<size_t>(response.size(), 150)));

        auto toolCall = parseToolCall(response);
        if (!toolCall.has_value()) {
            log().info("[AgentLoop] ✅ Resposta final na iteração {}.", iter);
            return {response, requiresAudio};
        }

        // Action
        log().info("[AgentLoop] 🔧 Action: tool='{}' args={}", toolCall->name,
            [&] {
                std::string s;
                for (auto& [k, v] : toolCall->arguments) s += k + "=" + v + " ";
                return s;
            }());

        BaseTool* tool = registry_->get(toolCall->name);
        std::string observation;

        if (!tool) {
            observation = "Ferramenta \"" + toolCall->name + "\" não encontrada.";
        } else {
            ToolResult res = tool->execute(toolCall->arguments);
            observation    = res.hasError() ? "Erro: " + res.error : res.output;
        }

        log().info("[AgentLoop] 👁️  Observation: {}...",
            observation.substr(0, std::min<size_t>(observation.size(), 150)));

        history.push_back(LlmMessage{"assistant", response});
        history.push_back(LlmMessage{"tool", "Resultado de \"" + toolCall->name + "\": " + observation});
    }

    log().warn("[AgentLoop] ⚠️ MAX_ITERATIONS ({}) atingido.", maxIterations_);
    return {
        "Desculpe, não consegui completar a tarefa dentro de " +
        std::to_string(maxIterations_) + " iterações. Por favor, reformule.",
        requiresAudio
    };
}

std::optional<AgentLoop::ToolCall> AgentLoop::parseToolCall(const std::string& response) {
    static const std::regex jsonRe(R"(\{[\s\S]*"tool_call"[\s\S]*\})");
    std::smatch match;
    if (!std::regex_search(response, match, jsonRe)) return std::nullopt;

    try {
        auto j = json::parse(match[0].str());
        if (!j.contains("tool_call")) return std::nullopt;

        ToolCall tc;
        tc.name = j["tool_call"].value("name", std::string{});
        if (tc.name.empty()) return std::nullopt;

        if (j["tool_call"].contains("arguments")) {
            for (const auto& [k, v] : j["tool_call"]["arguments"].items()) {
                tc.arguments[k] = v.is_string() ? v.get<std::string>() : v.dump();
            }
        }
        return tc;
    } catch (...) {
        return std::nullopt;
    }
}
