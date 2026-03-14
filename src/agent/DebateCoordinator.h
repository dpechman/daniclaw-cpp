#pragma once
#include "../providers/ILlmProvider.h"
#include "../tools/ToolRegistry.h"
#include "AgentLoop.h"
#include <string>
#include <memory>
#include <functional>

/**
 * DebateCoordinator — painel de múltiplos agentes especializados.
 *
 * Quando a pergunta exige pesquisa ou verificação factual, o debate acontece:
 *   1. Pesquisador  — usa todas as tools disponíveis para buscar informações.
 *   2. Crítico      — avalia a pesquisa, aponta falhas e adiciona contexto.
 *   3. Sintetizador — produz a resposta final consolidada e confiante.
 *
 * Para perguntas simples é mais eficiente cair direto no AgentLoop normal.
 * Use `needsDebate()` para classificar antes de chamar `run()`.
 */
class DebateCoordinator {
public:
    using NotifyFn = std::function<void(const std::string& msg)>;

    explicit DebateCoordinator(std::shared_ptr<ToolRegistry> registry);

    /**
     * Heurística rápida (sem API call) para decidir se vale o debate.
     * Retorna true para perguntas factuais, de pesquisa ou com muita incerteza.
     */
    bool needsDebate(const std::string& userMessage) const;

    /**
     * Executa o painel de 3 agentes e retorna a resposta final.
     * @param userMessage   A mensagem original do usuário.
     * @param basePrompt    System prompt base (timezone, chat_id, lembretes etc).
     * @param notify        Callback para enviar mensagens de progresso ao usuário.
     */
    std::string run(
        const std::string& userMessage,
        const std::string& basePrompt,
        NotifyFn notify = nullptr
    );

private:
    std::shared_ptr<ToolRegistry> registry_;

    // Executa um agente com AgentLoop (pode usar tools) e retorna a resposta.
    std::string runAgent(
        const std::string& systemPrompt,
        const std::string& userMessage,
        bool useTools = false
    );
};
