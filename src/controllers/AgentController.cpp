#include "AgentController.h"
#include "../config/Config.h"
#include "../utils/Logger.h"
#include "../tools/CreateFileTool.h"
#include "../tools/ReadFileTool.h"
#include "../tools/ScheduleReminderTool.h"
#include "../tools/SetTimezoneTool.h"
#include "../tools/ShellTool.h"
#include "../tools/WebSearchTool.h"
#include "../tools/SemanticSearchTool.h"
#include "../tools/IndexDocumentTool.h"
#include "../utils/Utils.h"
#include <sstream>
#include <filesystem>
#include <fstream>

AgentController::AgentController(TelegramBot& bot)
    : bot_(bot)
    , memory_(std::make_unique<MemoryManager>())
    , toolRegistry_(std::make_shared<ToolRegistry>())
{
    // Parse whitelist de IDs
    std::string rawIds = cfg().get("TELEGRAM_ALLOWED_USER_IDS");
    std::stringstream ss(rawIds);
    std::string id;
    while (std::getline(ss, id, ',')) {
        id.erase(0, id.find_first_not_of(" \t"));
        id.erase(id.find_last_not_of(" \t") + 1);
        if (!id.empty()) allowedUserIds_.insert(id);
    }

    // Registra ferramentas padrão
    toolRegistry_->registerTool(std::make_shared<CreateFileTool>());
    toolRegistry_->registerTool(std::make_shared<ReadFileTool>());
    toolRegistry_->registerTool(std::make_shared<ScheduleReminderTool>());
    toolRegistry_->registerTool(std::make_shared<SetTimezoneTool>());
    toolRegistry_->registerTool(std::make_shared<ShellTool>());
    toolRegistry_->registerTool(std::make_shared<WebSearchTool>());
    toolRegistry_->registerTool(std::make_shared<SemanticSearchTool>());
    toolRegistry_->registerTool(std::make_shared<IndexDocumentTool>());

    // Garante que pastas existam
    for (auto& dir : {"./tmp", "./output", "./data"}) {
        std::filesystem::create_directories(dir);
    }
}

void AgentController::registerHandlers() {
    bot_.onText([this](const Telegram::Message& msg) { handleText(msg); });
    bot_.onDocument([this](const Telegram::Message& msg) { handleDocument(msg); });
    bot_.onVoice([this](const Telegram::Message& msg) { handleVoice(msg); });
    log().info("[AgentController] Handlers registrados.");
}

bool AgentController::isAllowed(int64_t userId) {
    return allowedUserIds_.count(std::to_string(userId)) > 0;
}

bool AgentController::detectAudioRequest(const std::string& text) {
    static const std::vector<std::string> kw = {
        "responda em áudio", "fale comigo", "responde em áudio",
        "quero em áudio", "me responda em voz"
    };
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& k : kw) {
        if (lower.find(k) != std::string::npos) return true;
    }
    return false;
}

void AgentController::handleText(const Telegram::Message& msg) {
    if (!isAllowed(msg.from.id)) {
        log().warn("[Controller] Usuário não autorizado: {}", msg.from.id);
        return;
    }
    log().info("[Controller] Msg de {}: \"{}...\"", msg.from.id,
        msg.text.substr(0, std::min<size_t>(msg.text.size(), 80)));

    bool audio = detectAudioRequest(msg.text);
    bot_.sendChatAction(msg.chatId, "typing");
    processInput(msg.chatId, std::to_string(msg.from.id), msg.text, audio);
}

void AgentController::handleDocument(const Telegram::Message& msg) {
    if (!isAllowed(msg.from.id)) return;

    const auto& doc = msg.document;
    bool isPdf = (doc.mimeType == "application/pdf");
    bool isMd  = (doc.fileName.size() > 3 &&
                  doc.fileName.substr(doc.fileName.size() - 3) == ".md");

    if (!isPdf && !isMd) {
        bot_.sendMessage(msg.chatId, "📎 Só aceito arquivos PDF ou Markdown (.md).");
        return;
    }

    bot_.sendChatAction(msg.chatId, "typing");

    std::string tmpPath = "./tmp/doc_" + std::to_string(msg.messageId);
    if (!bot_.downloadFile(doc.fileId, tmpPath)) {
        bot_.sendMessage(msg.chatId, "⚠️ Falha ao baixar o arquivo.");
        return;
    }

    std::string content;
    try {
        if (isMd) {
            std::ifstream f(tmpPath);
            std::ostringstream ss;
            ss << f.rdbuf();
            content = ss.str();
        } else {
            // PDF: lê como texto bruto (para suporte completo instale pdftotext)
            content = "[PDF recebido — integração com pdftotext pendente]\n";
            content += "Arquivo: " + doc.fileName;
        }
        std::filesystem::remove(tmpPath);
    } catch (...) {
        std::filesystem::remove(tmpPath);
        bot_.sendMessage(msg.chatId, "⚠️ Erro ao processar o arquivo.");
        return;
    }

    if (!msg.caption.empty()) content = "Legenda: \"" + msg.caption + "\"\n\n" + content;
    processInput(msg.chatId, std::to_string(msg.from.id), content, false);
}

void AgentController::handleVoice(const Telegram::Message& msg) {
    if (!isAllowed(msg.from.id)) return;
    bot_.sendMessage(msg.chatId,
        "🎙️ Recebi seu áudio! A transcrição via Whisper requer configuração adicional. "
        "Por enquanto, envie sua mensagem por texto.");
}

void AgentController::processInput(int64_t chatId, const std::string& userId,
                                    const std::string& text, bool requiresAudio) {
    try {
        // 1. Persiste mensagem
        memory_->saveUserMessage(userId, text);

        // 2. Carrega skills (hot-reload)
        auto skills = skillLoader_.loadAll();

        // 3. Roteia skill
        auto skill = skillRouter_.route(text, skills);

        // 4. Monta system prompt
        int tzOffset      = Utils::timezoneOffsetHours();
        std::string tzSign = (tzOffset >= 0) ? "+" : "";
        std::string tzLabel = "UTC" + tzSign + std::to_string(tzOffset);

        std::string sysPrompt =
            "Você é o DaniClaw, um assistente de IA.\n\n"
            "INFORMAÇÃO IMPORTANTE DE CONTEXTO:\n"
            "- Fuso horário local configurado: " + tzLabel + "\n"
            "- Horário local atual: " + Utils::nowLocalISO() + "\n"
            "- Horário UTC atual: " + Utils::nowISO() + "\n"
            "- ID numérico do chat atual (`chat_id`): " + std::to_string(chatId) + "\n\n"
            "REGRAS DE FUSO HORÁRIO:\n"
            "- Todos os horários exibidos ao usuário devem usar o fuso local (" + tzLabel + ").\n"
            "- Se o usuário disser que está viajando, mudou de cidade/país, ou pedir para "
            "alterar o fuso, use IMEDIATAMENTE a ferramenta `definir_timezone` com o `offset_hours` correto.\n"
            "- Exemplos de offset: Brasil (BRT) = -3, Portugal/Londres (GMT) = 0, "
            "Europa Central (CET) = +1, Nova York (EST) = -5, Tóquio (JST) = +9.\n\n"
            "REGRAS DE LEMBRETES:\n"
            "- Sempre que o usuário pedir para ser lembrado de algo no futuro, "
            "use IMEDIATAMENTE a ferramenta `agendar_lembrete`.\n"
            "- Passe `chat_id` = " + std::to_string(chatId) + ".\n"
            "- Para intervalos relativos (ex: 'em 5 minutos', 'daqui a 1 hora'), "
            "use `delay_minutes` com o número de minutos. NÃO calcule ISO manualmente.\n"
            "- Só use `data_iso_utc` se o usuário informar um horário absoluto exato (já converta para UTC).\n"
            "- O campo `message` deve ser uma frase direta, ex: 'Tomar remédio'.";

        if (skill.has_value()) {
            sysPrompt += "\n\n## Skill Ativa: " + skill->name + "\n" + skill->content;
        }

        // 5. Contexto da memória
        auto context = memory_->getContext(userId);

        // 6. Executa AgentLoop
        auto provider = ProviderFactory::create();
        AgentLoop loop(provider, toolRegistry_);
        auto result = loop.run(context, sysPrompt, requiresAudio);

        // 7. Persiste resposta
        memory_->saveAssistantMessage(userId, result.answer);

        // 8. Envia resposta
        bot_.sendMessage(chatId, result.answer);

    } catch (const std::exception& e) {
        log().error("[AgentController] Erro no pipeline: {}", e.what());
        bot_.sendMessage(chatId, "⚠️ Erro interno. Tente novamente.");
    }
}
