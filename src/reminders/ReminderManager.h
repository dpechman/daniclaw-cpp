#pragma once
#include "../telegram/TelegramBot.h"
#include <thread>
#include <atomic>
#include <vector>
#include <string>

/**
 * Gerenciador de lembretes rodando em background.
 * Checa o SQLite periodicamente e dispara mensagens para o Telegram.
 */
class ReminderManager {
public:
    explicit ReminderManager(TelegramBot& bot);
    ~ReminderManager();

    void start(int checkIntervalMs = 5000);
    void stop();

    // Utilitário para salvar um lembrete manualmente
    static bool schedule(const std::string& chatId, const std::string& message, const std::string& remindAtISO);

private:
    TelegramBot& bot_;
    std::atomic<bool> running_{false};
    std::thread worker_;

    void loop(int intervalMs);
    void checkAndFire();
};
