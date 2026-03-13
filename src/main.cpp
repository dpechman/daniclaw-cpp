#include <iostream>
#include <csignal>
#include <curl/curl.h>
#include "config/Config.h"
#include "utils/Logger.h"
#include "database/Database.h"
#include "rag/VectorStore.h"
#include "telegram/TelegramBot.h"

// sqlite-vec: registra extensão antes de qualquer sqlite3_open
#define SQLITE_CORE 1
extern "C" {
#include "vendor/sqlite-vec/sqlite-vec.h"
}
namespace { const auto _vecReg = (sqlite3_auto_extension(reinterpret_cast<void(*)(void)>(sqlite3_vec_init)), 0); }
#include "controllers/AgentController.h"
#include "reminders/ReminderManager.h"

static TelegramBot* gBot = nullptr;
static ReminderManager* gRM = nullptr;

void signalHandler(int signum) {
    log().info("🛑 Sinal {} recebido. Encerrando DaniClaw...", signum);
    if (gRM) gRM->stop();
    if (gBot) gBot->stop();
}

int main() {
    // Carrega .env
    cfg().load(".env");

    log().info("🚀 Iniciando DaniClaw C++...");

    // Log do provider e modelo configurados
    {
        std::string provider = cfg().get("LLM_PROVIDER", "groq");
        std::string modelKey = provider + "_MODEL";
        // Normaliza para uppercase para buscar a chave
        for (auto& c : modelKey) c = ::toupper(c);
        std::string model = cfg().get(modelKey);
        if (model.empty()) {
            // fallback: tenta com o nome original
            std::string mk2 = provider + "_MODEL";
            mk2[0] = ::toupper(mk2[0]);
            model = cfg().get(mk2, "?");
        }
        log().info("🧠 Provider: {}  |  Modelo: {}", provider, model);
    }

    // Inicializa libcurl globalmente
    curl_global_init(CURL_GLOBAL_ALL);

    try {
        // Banco de dados
        Database::getInstance().initialize();

        // RAG — base de conhecimento vetorial
        VectorStore::initialize(cfg().getInt("EMBEDDING_DIMS", 1536));

        // Token do bot
        std::string token = cfg().get("TELEGRAM_BOT_TOKEN");
        if (token.empty()) {
            log().error("❌ TELEGRAM_BOT_TOKEN não definido no .env");
            return 1;
        }

        // Bot + Controller
        TelegramBot bot(token);
        gBot = &bot;

        AgentController controller(bot);
        controller.registerHandlers();

        // Inicia sistema de lembretes em background (checa o BD a cada 10 seg)
        ReminderManager rm(bot);
        gRM = &rm;
        rm.start(10000);

        // Graceful shutdown
        std::signal(SIGINT,  signalHandler);
        std::signal(SIGTERM, signalHandler);

        log().info("🤖 DaniClaw C++ online! Aguardando mensagens no Telegram...");
        bot.start(); // bloqueante

    } catch (const std::exception& e) {
        log().error("❌ Erro fatal: {}", e.what());
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    log().info("👋 DaniClaw encerrado.");
    return 0;
}
