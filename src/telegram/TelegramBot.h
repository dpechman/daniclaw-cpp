#pragma once
#include "TelegramTypes.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

/**
 * Cliente Telegram Bot com long-polling manual via libcurl + nlohmann/json.
 * Emula a interface de um bot grammy simplificado.
 */
class TelegramBot {
public:
    using MessageHandler = std::function<void(const Telegram::Message&)>;

    explicit TelegramBot(const std::string& token);

    // Registra handlers por tipo
    void onText(MessageHandler handler)     { textHandler_     = std::move(handler); }
    void onDocument(MessageHandler handler) { documentHandler_ = std::move(handler); }
    void onVoice(MessageHandler handler)    { voiceHandler_    = std::move(handler); }

    // Inicia o loop de polling (bloqueante)
    void start();
    void stop() { running_ = false; }

    // Métodos de envio
    bool sendMessage(int64_t chatId, const std::string& text);
    bool sendDocument(int64_t chatId, const std::string& filePath, const std::string& caption = "");
    bool sendVoice(int64_t chatId, const std::string& filePath);
    bool sendChatAction(int64_t chatId, const std::string& action);

    // Download de arquivo
    std::string getFileUrl(const std::string& fileId);
    bool downloadFile(const std::string& fileId, const std::string& destPath);

private:
    std::string token_;
    std::string apiBase_;
    std::atomic<bool> running_{false};
    int64_t offset_{0};

    MessageHandler textHandler_;
    MessageHandler documentHandler_;
    MessageHandler voiceHandler_;

    std::vector<Telegram::Update> getUpdates(int64_t offset, int timeout = 30);
    Telegram::Update parseUpdate(const nlohmann::json& j);
    void handleUpdate(const Telegram::Update& update);

    std::string endpoint(const std::string& method) const;
};
