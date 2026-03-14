#include "DebateCoordinator.h"
#include "AgentLoop.h"
#include "../providers/ProviderFactory.h"
#include "../config/Config.h"
#include "../utils/Logger.h"
#include <algorithm>
#include <cctype>

DebateCoordinator::DebateCoordinator(std::shared_ptr<ToolRegistry> registry)
    : registry_(std::move(registry))
{}

bool DebateCoordinator::needsDebate(const std::string& userMessage) const {
    if (cfg().get("MULTI_AGENT_ENABLED", "true") == "false") return false;

    std::string lower = userMessage;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Gatilhos de pesquisa factual externa
    static const std::vector<std::string> factualTriggers = {
        "quanto custa", "qual o preco", "qual o valor", "preco de", "valor de",
        "quando abre", "quando fecha", "horario", "horário",
        "previsao do tempo", "previsão do tempo", "clima", "temperatura",
        "noticia", "notícia", "noticias", "notícias",
        "quem é", "quem e ", "o que é", "o que e ",
        "onde fica", "como chegar", "endereco", "endereço",
        "resultado", "placar", "campeonato",
        "cotacao", "cotação", "cambio", "câmbio", "dolar", "dólar",
        "passagem", "voo", "hotel",
        "pesquise", "pesquisa", "busque", "busca",
        "qual a ultima", "última notícia", "recente",
        "me diga sobre", "me fale sobre", "informacoes sobre", "informações sobre",
    };

    for (const auto& t : factualTriggers) {
        if (lower.find(t) != std::string::npos) return true;
    }
    return false;
}

std::string DebateCoordinator::runAgent(
    const std::string& systemPrompt,
    const std::string& userMessage,
    bool useTools
) {
    auto provider = ProviderFactory::create();
    auto registry = useTools ? registry_ : std::make_shared<ToolRegistry>();
    AgentLoop loop(provider, registry);
    std::vector<LlmMessage> msgs = {{"user", userMessage}};
    auto result = loop.run(msgs, systemPrompt, false);
    return result.answer;
}

std::string DebateCoordinator::run(
    const std::string& userMessage,
    const std::string& basePrompt,
    NotifyFn notify
) {
    log().info("[Debate] Iniciando painel de 3 agentes para: {}...",
        userMessage.substr(0, 80));

    // ── AGENTE 1: Pesquisador ───────────────────────────────────────────────
    if (notify) notify("🔎 Consultando especialistas...");

    std::string pesquisadorPrompt =
        basePrompt + "\n\n"
        "PAPEL: Você é o Agente Pesquisador.\n"
        "MISSÃO: Responda à pergunta do usuário buscando informações REAIS e ATUAIS.\n"
        "- Use OBRIGATORIAMENTE a ferramenta `pesquisar_internet` para buscar dados reais.\n"
        "- Seja detalhado: inclua valores, fontes, datas e contexto encontrados.\n"
        "- NÃO invente dados. Se não encontrar, informe explicitamente o que encontrou e o que não encontrou.\n"
        "- Finalize com um resumo estruturado das informações coletadas.";

    std::string pesquisaResult = runAgent(pesquisadorPrompt, userMessage, true);
    log().info("[Debate] Pesquisador concluiu ({} chars).", pesquisaResult.size());

    // ── AGENTE 2: Crítico ───────────────────────────────────────────────────
    if (notify) notify("🧐 Verificando as informações...");

    std::string criticoPrompt =
        basePrompt + "\n\n"
        "PAPEL: Você é o Agente Crítico/Verificador.\n"
        "MISSÃO: Analise a pesquisa abaixo e avalie:\n"
        "1. As informações parecem corretas, coerentes e atuais?\n"
        "2. Há lacunas, contradições ou incertezas que o usuário deve saber?\n"
        "3. Adicione contexto ou ressalvas importantes que o Pesquisador não mencionou.\n"
        "4. Se a pesquisa estiver completa e correta, confirme isso explicitamente.\n"
        "Seja objetivo e direto. NÃO repita tudo o que o Pesquisador disse.\n\n"
        "PESQUISA DO AGENTE PESQUISADOR:\n" + pesquisaResult;

    std::string criticaResult = runAgent(criticoPrompt, userMessage, false);
    log().info("[Debate] Crítico concluiu ({} chars).", criticaResult.size());

    // ── AGENTE 3: Sintetizador ──────────────────────────────────────────────
    if (notify) notify("✍️ Consolidando resposta...");

    std::string sintetizadorPrompt =
        basePrompt + "\n\n"
        "PAPEL: Você é o Agente Sintetizador.\n"
        "MISSÃO: Com base na pesquisa e na análise crítica abaixo, produza a RESPOSTA FINAL para o usuário.\n"
        "- Seja claro, direto e confiante.\n"
        "- Incorpore as ressalvas do Crítico quando relevantes, mas sem ser alarmista.\n"
        "- Formate bem a resposta (use listas, negrito etc. quando ajudar).\n"
        "- NÃO mencione os 'agentes', 'pesquisador', 'crítico' — a resposta deve parecer natural.\n"
        "- Se houver incerteza genuína, declare claramente ao invés de afirmar algo errado.\n\n"
        "PESQUISA:\n" + pesquisaResult + "\n\n"
        "ANÁLISE CRÍTICA:\n" + criticaResult;

    std::string finalAnswer = runAgent(sintetizadorPrompt, userMessage, false);
    log().info("[Debate] Sintetizador concluiu. Resposta final ({} chars).", finalAnswer.size());

    return finalAnswer;
}
