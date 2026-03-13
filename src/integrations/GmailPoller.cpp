#include "GmailPoller.h"
#include "../config/Config.h"
#include "../database/Database.h"
#include "../utils/Logger.h"
#include "../utils/Utils.h"
#include <curl/curl.h>
#include <sqlite3.h>
#include <sstream>
#include <regex>
#include <algorithm>
#include <stdexcept>

// ── libcurl write callback ────────────────────────────────────────────────────

static size_t imapWriteCallback(void* ptr, size_t size, size_t nmemb, std::string* out) {
    out->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

GmailPoller::GmailPoller(TelegramBot& bot) : bot_(bot) {
    user_        = cfg().get("GMAIL_USER");
    password_    = cfg().get("GMAIL_APP_PASSWORD");
    intervalSec_ = cfg().getInt("GMAIL_POLL_INTERVAL_SEC", 60);

    std::string rawChats = cfg().get("GMAIL_NOTIFY_CHAT_IDS");
    std::istringstream ss(rawChats);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) {
            try { notifyChats_.push_back(std::stoll(token)); } catch (...) {}
        }
    }
}

GmailPoller::~GmailPoller() { stop(); }

// ── Start / Stop ──────────────────────────────────────────────────────────────

void GmailPoller::start() {
    if (user_.empty() || password_.empty()) {
        log().warn("[GmailPoller] GMAIL_USER ou GMAIL_APP_PASSWORD não configurados. Poller desabilitado.");
        return;
    }
    if (notifyChats_.empty()) {
        log().warn("[GmailPoller] GMAIL_NOTIFY_CHAT_IDS não configurado. Poller desabilitado.");
        return;
    }
    initDb();
    running_ = true;
    worker_  = std::thread(&GmailPoller::loop, this);
    log().info("📧 GmailPoller iniciado para {} (intervalo: {}s)", user_, intervalSec_);
}

void GmailPoller::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void GmailPoller::loop() {
    while (running_) {
        try {
            poll();
        } catch (const std::exception& e) {
            log().error("[GmailPoller] Erro no poll: {}", e.what());
        }
        for (int i = 0; i < intervalSec_ * 10 && running_; ++i) {
            Utils::sleepMs(100);
        }
    }
}

void GmailPoller::poll() {
    // 1. SEARCH UNSEEN
    std::string searchResp = imapCommand("UID SEARCH UNSEEN");
    auto uids = parseSearchResult(searchResp);

    if (uids.empty()) {
        log().debug("[GmailPoller] Nenhum email não lido.");
        return;
    }

    log().info("[GmailPoller] {} email(s) não lido(s) encontrado(s).", uids.size());

    for (const auto& uid : uids) {
        if (alreadySeen(uid)) continue;

        try {
            std::string raw   = fetchMessage(uid);
            EmailMessage email = parseEmail(uid, raw);
            notifyTelegram(email);
            markSeen(uid);
        } catch (const std::exception& e) {
            log().warn("[GmailPoller] Erro ao processar UID {}: {}", uid, e.what());
        }
    }
}

// ── IMAP via libcurl ──────────────────────────────────────────────────────────

std::string GmailPoller::imapCommand(const std::string& customRequest,
                                      const std::string& mailboxUrl) {
    std::string url = mailboxUrl.empty()
        ? "imaps://imap.gmail.com/INBOX"
        : mailboxUrl;

    std::string response;
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init falhou.");

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME,      user_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD,      password_.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, customRequest.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, imapWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);
    curl_easy_setopt(curl, CURLOPT_USE_SSL,       CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       30L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("IMAP error: ") + curl_easy_strerror(res));
    }
    return response;
}

std::string GmailPoller::fetchMessage(const std::string& uid) {
    // Busca headers relevantes + primeiros 600 bytes do body
    std::string cmd =
        "UID FETCH " + uid +
        " (BODY[HEADER.FIELDS (FROM SUBJECT DATE)] BODY[TEXT]<0.600>)";
    return imapCommand(cmd);
}

// ── Parsing ───────────────────────────────────────────────────────────────────

std::vector<std::string> GmailPoller::parseSearchResult(const std::string& response) {
    // Resposta: "* SEARCH 1 3 5 27\r\n"
    std::vector<std::string> uids;
    std::regex re(R"(\* SEARCH([\d\s]*))");
    std::smatch m;
    if (!std::regex_search(response, m, re)) return uids;

    std::string nums = m[1].str();
    std::istringstream ss(nums);
    std::string n;
    while (ss >> n) {
        if (!n.empty()) uids.push_back(n);
    }
    return uids;
}

std::string GmailPoller::extractHeader(const std::string& raw, const std::string& field) {
    // Busca "Field: valor\r\n" (case-insensitive)
    std::regex re("(?i)" + field + R"(:\s*([^\r\n]+))", std::regex::icase);
    std::smatch m;
    if (std::regex_search(raw, m, re)) {
        std::string val = m[1].str();
        // Trim
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t\r\n") + 1);
        return val;
    }
    return "";
}

std::string GmailPoller::extractBodySnippet(const std::string& raw) {
    // Após headers (linha em branco), pega o body
    auto pos = raw.find("\r\n\r\n");
    if (pos == std::string::npos) pos = raw.find("\n\n");
    if (pos == std::string::npos) return "";

    std::string body = raw.substr(pos + 4);

    // Remove tags HTML básicas
    body = std::regex_replace(body, std::regex("<[^>]+>"), " ");
    // Colapsa whitespace
    body = std::regex_replace(body, std::regex(R"([\r\n\t ]+)"), " ");
    // Trim
    body.erase(0, body.find_first_not_of(" \t"));

    if (body.size() > 300) body = body.substr(0, 300) + "...";
    return body;
}

EmailMessage GmailPoller::parseEmail(const std::string& uid, const std::string& raw) {
    EmailMessage e;
    e.uid     = uid;
    e.from    = extractHeader(raw, "From");
    e.subject = extractHeader(raw, "Subject");
    e.date    = extractHeader(raw, "Date");
    e.snippet = extractBodySnippet(raw);
    return e;
}

// ── Notificação ───────────────────────────────────────────────────────────────

void GmailPoller::notifyTelegram(const EmailMessage& email) {
    std::ostringstream msg;
    msg << "📧 *Novo e-mail recebido*\n\n"
        << "👤 *De:* " << email.from    << "\n"
        << "📌 *Assunto:* " << email.subject << "\n"
        << "📅 *Data:* " << email.date   << "\n";
    if (!email.snippet.empty()) {
        msg << "\n💬 " << email.snippet;
    }

    std::string text = msg.str();
    for (int64_t chatId : notifyChats_) {
        log().info("[GmailPoller] Notificando chat {} — {}", chatId, email.subject);
        bot_.sendMessage(chatId, text);
    }
}

// ── SQLite: controle de UIDs já processados ───────────────────────────────────

void GmailPoller::initDb() {
    sqlite3* db = Database::getInstance().getDb();
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS gmail_seen (
            uid        TEXT PRIMARY KEY,
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )";
    char* err = nullptr;
    sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

bool GmailPoller::alreadySeen(const std::string& uid) {
    sqlite3* db = Database::getInstance().getDb();
    const char* sql = "SELECT 1 FROM gmail_seen WHERE uid = ?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, SQLITE_TRANSIENT);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

void GmailPoller::markSeen(const std::string& uid) {
    sqlite3* db = Database::getInstance().getDb();
    const char* sql = "INSERT OR IGNORE INTO gmail_seen(uid) VALUES (?)";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
