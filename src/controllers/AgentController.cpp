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
#include "../agent/DebateCoordinator.h"
#include "../utils/Utils.h"
#include <sstream>
#include <filesystem>
#include <fstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <chrono>

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

    // Registra ferramentas padrão (cada uma pode ser desabilitada no .env)
    auto enabled = [](const std::string& key) {
        return cfg().get(key, "true") != "false";
    };

    if (enabled("TOOL_FILE_ENABLED")) {
        toolRegistry_->registerTool(std::make_shared<CreateFileTool>());
        toolRegistry_->registerTool(std::make_shared<ReadFileTool>());
    }
    if (enabled("TOOL_REMINDERS_ENABLED")) {
        toolRegistry_->registerTool(std::make_shared<ScheduleReminderTool>());
    }
    if (enabled("TOOL_TIMEZONE_ENABLED")) {
        toolRegistry_->registerTool(std::make_shared<SetTimezoneTool>());
    }
    if (enabled("TOOL_SHELL_ENABLED")) {
        toolRegistry_->registerTool(std::make_shared<ShellTool>());
    }
    if (enabled("TOOL_WEB_SEARCH_ENABLED")) {
        toolRegistry_->registerTool(std::make_shared<WebSearchTool>());
    }
    if (enabled("TOOL_RAG_ENABLED")) {
        toolRegistry_->registerTool(std::make_shared<SemanticSearchTool>());
        toolRegistry_->registerTool(std::make_shared<IndexDocumentTool>());
    }

    // Garante que pastas existam
    for (auto& dir : {"./tmp", "./output", "./data"}) {
        std::filesystem::create_directories(dir);
    }
}

void AgentController::registerHandlers() {
    auto enabled = [](const std::string& key) {
        return cfg().get(key, "true") != "false";
    };

    bot_.onText([this](const Telegram::Message& msg) { handleText(msg); });
    if (enabled("DOCUMENT_ENABLED")) {
        bot_.onDocument([this](const Telegram::Message& msg) { handleDocument(msg); });
    }
    if (enabled("VOICE_ENABLED")) {
        bot_.onVoice([this](const Telegram::Message& msg) { handleVoice(msg); });
    }
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
    if (cfg().get("TTS_ENABLED", "true") == "false") audio = false;
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

    const std::string fileId = msg.hasVoice ? msg.voice.fileId : msg.audio.fileId;
    if (fileId.empty()) return;

    bot_.sendChatAction(msg.chatId, "record_voice");

    std::string tmpPath = "./tmp/voice_" + std::to_string(msg.messageId) + ".oga";
    if (!bot_.downloadFile(fileId, tmpPath)) {
        bot_.sendMessage(msg.chatId, "⚠️ Falha ao baixar o áudio.");
        return;
    }

    std::string transcript = transcribeAudio(tmpPath);
    std::filesystem::remove(tmpPath);

    if (transcript.empty()) {
        bot_.sendMessage(msg.chatId, "⚠️ Não consegui transcrever o áudio. Tente novamente.");
        return;
    }

    log().info("[Voice] Transcrição: {}", transcript.substr(0, 120));
    // Áudio de entrada → responde em áudio automaticamente
    bool audioReply = cfg().get("TTS_ENABLED", "true") != "false";
    processInput(msg.chatId, std::to_string(msg.from.id), transcript, audioReply);
}

std::string AgentController::transcribeAudio(const std::string& filePath) {
    using json = nlohmann::json;

    std::string apiKey = cfg().get("OPENAI_API_KEY");
    if (apiKey.empty()) {
        log().error("[STT] OPENAI_API_KEY não configurada.");
        return "";
    }
    std::string model = cfg().get("WHISPER_MODEL", "whisper-1");
    std::string language = cfg().get("WHISPER_LANGUAGE", "pt");

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    struct curl_mime* form = curl_mime_init(curl);
    curl_mimepart* field;

    field = curl_mime_addpart(form);
    curl_mime_name(field, "model");
    curl_mime_data(field, model.c_str(), CURL_ZERO_TERMINATED);

    if (!language.empty()) {
        field = curl_mime_addpart(form);
        curl_mime_name(field, "language");
        curl_mime_data(field, language.c_str(), CURL_ZERO_TERMINATED);
    }

    field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, filePath.c_str());

    std::string authHeader = "Authorization: Bearer " + apiKey;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());

    std::string responseBody;
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/audio/transcriptions");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](void* ptr, size_t sz, size_t nm, std::string* s) -> size_t {
            s->append(static_cast<char*>(ptr), sz * nm);
            return sz * nm;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode code = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_mime_free(form);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        log().error("[STT] curl error: {}", curl_easy_strerror(code));
        return "";
    }

    try {
        auto j = json::parse(responseBody);
        if (j.contains("text")) return j["text"].get<std::string>();
        if (j.contains("error")) {
            log().error("[STT] API error: {}", j["error"]["message"].get<std::string>());
        }
    } catch (...) {
        log().error("[STT] Falha ao parsear resposta Whisper.");
    }
    return "";
}

void AgentController::processInput(int64_t chatId, const std::string& userId,
                                    const std::string& text, bool requiresAudio) {
    try {
        // 1. Persiste mensagem
        if (cfg().get("MEMORY_ENABLED", "true") != "false") {
            memory_->saveUserMessage(userId, text);
        }

        // 2. Carrega skills (hot-reload)
        std::vector<SkillMeta> skills;
        std::optional<SkillMeta> skill;
        if (cfg().get("SKILLS_ENABLED", "true") != "false") {
            skills = skillLoader_.loadAll();

        // 3. Roteia skill
            skill = skillRouter_.route(text, skills);
        }

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
            "- O campo `message` deve ser uma frase direta, ex: 'Tomar remedio'.\n\n"
            "REGRAS DE COMPORTAMENTO:\n"
            "- NUNCA responda com frases do tipo 'vou verificar', 'um momento', 'aguarde' "
            "sem IMEDIATAMENTE chamar a ferramenta necessaria no mesmo turno.\n"
            "- Se precisar buscar informacao, executar um comando ou qualquer acao externa, "
            "chame a ferramenta AGORA. Responda ao usuario SOMENTE quando tiver a resposta completa.\n"
            "- Se nao for possivel obter a informacao, diga diretamente o motivo sem rodeios.";

        if (skill.has_value()) {
            sysPrompt += "\n\n## Skill Ativa: " + skill->name + "\n" + skill->content;
        }

        // 5. Contexto da memória
        auto context = (cfg().get("MEMORY_ENABLED", "true") != "false")
            ? memory_->getContext(userId)
            : std::vector<LlmMessage>{{"user", text}};

        // 6. Decide: debate multi-agente ou loop simples
        std::string finalAnswer;

        DebateCoordinator debate(toolRegistry_);
        if (debate.needsDebate(text)) {
            log().info("[AgentController] Usando painel multi-agente.");
            finalAnswer = debate.run(text, sysPrompt,
                [this, chatId](const std::string& msg) {
                    bot_.sendChatAction(chatId, "typing");
                    bot_.sendMessage(chatId, msg);
                });
        } else {
            // Loop ReAct simples
            auto provider = ProviderFactory::create();
            AgentLoop loop(provider, toolRegistry_);

            static const std::map<std::string, std::string> kToolMsg = {
                {"pesquisar_internet",  "\xf0\x9f\x94\x8d Pesquisando na internet..."},
                {"executar_comando",    "\xe2\x9a\x99\xef\xb8\x8f Executando comando..."},
                {"busca_semantica",     "\xf0\x9f\xa7\xa0 Consultando base de conhecimento..."},
                {"indexar_documento",   "\xf0\x9f\x93\x9a Indexando documento..."},
            };
            loop.setStepCallback([this, chatId](const std::string& toolName, int /*iter*/) {
                bot_.sendChatAction(chatId, "typing");
                auto it = kToolMsg.find(toolName);
                if (it != kToolMsg.end()) bot_.sendMessage(chatId, it->second);
            });

            std::atomic<bool> loopDone{false};
            std::thread typingThread([this, chatId, &loopDone]() {
                while (!loopDone.load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(4));
                    if (!loopDone.load()) bot_.sendChatAction(chatId, "typing");
                }
            });
            struct ThreadGuard {
                std::thread& t; std::atomic<bool>& done;
                ~ThreadGuard() { done.store(true); if (t.joinable()) t.join(); }
            } guard{typingThread, loopDone};

            auto result = loop.run(context, sysPrompt, requiresAudio);
            loopDone.store(true);
            typingThread.join();
            finalAnswer = result.answer;
        }

        // 7. Persiste resposta
        if (cfg().get("MEMORY_ENABLED", "true") != "false") {
            memory_->saveAssistantMessage(userId, finalAnswer);
        }

        // 8. Envia resposta
        bot_.sendMessage(chatId, finalAnswer);

    } catch (const std::exception& e) {
        log().error("[AgentController] Erro no pipeline: {}", e.what());
        bot_.sendMessage(chatId, "⚠️ Erro interno. Tente novamente.");
    }
}
