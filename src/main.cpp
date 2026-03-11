#include <iostream>
#include <csignal>
#include <curl/curl.h>
#include "config/Config.h"
#include "utils/Logger.h"
#include "database/Database.h"
#include "telegram/TelegramBot.h"
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

    // Inicializa libcurl globalmente
    curl_global_init(CURL_GLOBAL_ALL);

    try {
        // Banco de dados
        Database::getInstance().initialize();

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
