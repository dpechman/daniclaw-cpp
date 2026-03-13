#pragma once
#include "../telegram/TelegramBot.h"
#include <thread>
#include <atomic>
#include <string>
#include <vector>

struct EmailMessage {
    std::string uid;
    std::string from;
    std::string subject;
    std::string date;
    std::string snippet;  // primeiros ~400 chars do body
};

/**
 * Monitora a caixa de entrada do Gmail via IMAPS (IMAP sobre TLS).
 * Roda em background e notifica usuários do Telegram ao detectar emails não lidos.
 *
 * Configuração no .env:
 *   GMAIL_USER=seu@gmail.com
 *   GMAIL_APP_PASSWORD=xxxx xxxx xxxx xxxx   (App Password de 16 chars)
 *   GMAIL_NOTIFY_CHAT_IDS=123456789,987654321
 *   GMAIL_POLL_INTERVAL_SEC=60
 *   GMAIL_ENABLED=true
 */
class GmailPoller {
public:
    explicit GmailPoller(TelegramBot& bot);
    ~GmailPoller();

    void start();
    void stop();

private:
    TelegramBot&      bot_;
    std::atomic<bool> running_{false};
    std::thread       worker_;
    std::string       user_;
    std::string       password_;
    std::vector<int64_t> notifyChats_;
    int               intervalSec_{60};

    void loop();
    void poll();

    // IMAP helpers via libcurl
    std::string imapCommand(const std::string& customRequest, const std::string& mailboxUrl = "");
    std::string fetchMessage(const std::string& uid);

    // Parsing
    std::vector<std::string> parseSearchResult(const std::string& response);
    EmailMessage             parseEmail(const std::string& uid, const std::string& raw);
    std::string              extractHeader(const std::string& raw, const std::string& field);
    std::string              extractBodySnippet(const std::string& raw);
    std::string              decodeQuotedPrintable(const std::string& s);
    std::string              decodeBase64Line(const std::string& s);

    // Persistência de UIDs já processados
    void initDb();
    bool alreadySeen(const std::string& uid);
    void markSeen(const std::string& uid);

    void notifyTelegram(const EmailMessage& email);
};
