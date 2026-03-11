#include "TelegramBot.h"
#include "../utils/HttpClient.h"
#include "../utils/Logger.h"
#include "../utils/Utils.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

TelegramBot::TelegramBot(const std::string& token)
    : token_(token)
    , apiBase_("https://api.telegram.org/bot" + token)
{}

std::string TelegramBot::endpoint(const std::string& method) const {
    return apiBase_ + "/" + method;
}

void TelegramBot::start() {
    running_ = true;
    log().info("🤖 Telegram long-polling iniciado...");

    while (running_) {
        try {
            auto updates = getUpdates(offset_);
            for (const auto& upd : updates) {
                if (upd.updateId >= offset_) {
                    offset_ = upd.updateId + 1;
                }
                handleUpdate(upd);
            }
        } catch (const std::exception& e) {
            log().error("[TelegramBot] Erro no loop: {}", e.what());
            Utils::sleepMs(3000);
        }
    }
}

std::vector<Telegram::Update> TelegramBot::getUpdates(int64_t offset, int timeout) {
    std::string url = endpoint("getUpdates") +
        "?offset=" + std::to_string(offset) +
        "&timeout=" + std::to_string(timeout);

    auto res = HttpClient::get(url);
    if (!res.ok() || res.body.empty()) return {};

    try {
        auto j = json::parse(res.body);
        if (!j.value("ok", false)) return {};

        std::vector<Telegram::Update> updates;
        for (const auto& item : j["result"]) {
            updates.push_back(parseUpdate(item));
        }
        return updates;
    } catch (...) {
        return {};
    }
}

Telegram::Update TelegramBot::parseUpdate(const json& j) {
    Telegram::Update upd;
    upd.updateId = j.value("update_id", int64_t(0));

    if (!j.contains("message")) return upd;
    upd.hasMessage = true;

    const auto& m = j["message"];
    upd.message.messageId = m.value("message_id", int64_t(0));
    upd.message.chatId    = m.contains("chat") ? m["chat"].value("id", int64_t(0)) : 0;

    if (m.contains("from")) {
        upd.message.from.id        = m["from"].value("id", int64_t(0));
        upd.message.from.firstName = m["from"].value("first_name", std::string{});
        upd.message.from.username  = m["from"].value("username",   std::string{});
    }

    if (m.contains("text")) {
        upd.message.text    = m["text"].get<std::string>();
        upd.message.hasText = true;
    }

    if (m.contains("caption")) {
        upd.message.caption = m["caption"].get<std::string>();
    }

    if (m.contains("document")) {
        const auto& d = m["document"];
        upd.message.document.fileId   = d.value("file_id",   std::string{});
        upd.message.document.fileName = d.value("file_name", std::string{});
        upd.message.document.mimeType = d.value("mime_type", std::string{});
        upd.message.hasDocument = true;
    }

    if (m.contains("voice")) {
        upd.message.voice.fileId   = m["voice"].value("file_id", std::string{});
        upd.message.voice.duration = m["voice"].value("duration", 0);
        upd.message.hasVoice = true;
    }

    if (m.contains("audio")) {
        upd.message.audio.fileId   = m["audio"].value("file_id", std::string{});
        upd.message.hasAudio = true;
    }

    return upd;
}

void TelegramBot::handleUpdate(const Telegram::Update& upd) {
    if (!upd.hasMessage) return;
    const auto& msg = upd.message;

    if (msg.hasVoice || msg.hasAudio) {
        if (voiceHandler_) voiceHandler_(msg);
    } else if (msg.hasDocument) {
        if (documentHandler_) documentHandler_(msg);
    } else if (msg.hasText) {
        if (textHandler_) textHandler_(msg);
    }
}

bool TelegramBot::sendMessage(int64_t chatId, const std::string& text) {
    // Limita a 4096 chars por mensagem
    const size_t maxLen = 4096;
    std::string remaining = text;

    while (!remaining.empty()) {
        std::string chunk;
        if (remaining.size() <= maxLen) {
            chunk = remaining;
            remaining.clear();
        } else {
            size_t splitAt = remaining.rfind('\n', maxLen);
            if (splitAt == std::string::npos) splitAt = remaining.rfind(' ', maxLen);
            if (splitAt == std::string::npos) splitAt = maxLen;
            chunk = remaining.substr(0, splitAt);
            remaining = remaining.substr(splitAt);
            while (!remaining.empty() && (remaining[0] == '\n' || remaining[0] == ' '))
                remaining.erase(0, 1);
        }

        json body = {
            {"chat_id", chatId},
            {"text",    chunk}
        };
        auto res = HttpClient::post(endpoint("sendMessage"), body.dump());
        if (!res.ok()) {
            log().error("[TelegramBot] sendMessage falhou: {}", res.body);
            return false;
        }
        if (!remaining.empty()) Utils::sleepMs(500);
    }
    return true;
}

bool TelegramBot::sendChatAction(int64_t chatId, const std::string& action) {
    json body = {{"chat_id", chatId}, {"action", action}};
    auto res  = HttpClient::post(endpoint("sendChatAction"), body.dump());
    return res.ok();
}

std::string TelegramBot::getFileUrl(const std::string& fileId) {
    auto res = HttpClient::get(endpoint("getFile") + "?file_id=" + fileId);
    if (!res.ok()) return "";
    try {
        auto j = json::parse(res.body);
        if (!j.value("ok", false)) return "";
        std::string filePath = j["result"].value("file_path", std::string{});
        return "https://api.telegram.org/file/bot" + token_ + "/" + filePath;
    } catch (...) {
        return "";
    }
}

bool TelegramBot::downloadFile(const std::string& fileId, const std::string& destPath) {
    std::string url = getFileUrl(fileId);
    if (url.empty()) return false;
    return HttpClient::downloadFile(url, destPath);
}

bool TelegramBot::sendDocument(int64_t chatId, const std::string& filePath, const std::string& caption) {
    // Usa multipart/form-data via curl diretamente
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    struct curl_mime* form    = curl_mime_init(curl);
    struct curl_mimepart* field;

    // chat_id
    field = curl_mime_addpart(form);
    curl_mime_name(field, "chat_id");
    curl_mime_data(field, std::to_string(chatId).c_str(), CURL_ZERO_TERMINATED);

    // document
    field = curl_mime_addpart(form);
    curl_mime_name(field, "document");
    curl_mime_filedata(field, filePath.c_str());

    if (!caption.empty()) {
        field = curl_mime_addpart(form);
        curl_mime_name(field, "caption");
        curl_mime_data(field, caption.c_str(), CURL_ZERO_TERMINATED);
    }

    std::string responseBody;
    curl_easy_setopt(curl, CURLOPT_URL, endpoint("sendDocument").c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t sz, size_t nm, std::string* s) -> size_t {
        s->append(static_cast<char*>(ptr), sz * nm);
        return sz * nm;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    CURLcode code = curl_easy_perform(curl);
    curl_mime_free(form);
    curl_easy_cleanup(curl);
    return (code == CURLE_OK);
}

bool TelegramBot::sendVoice(int64_t chatId, const std::string& filePath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    struct curl_mime* form = curl_mime_init(curl);
    struct curl_mimepart* field;

    field = curl_mime_addpart(form);
    curl_mime_name(field, "chat_id");
    curl_mime_data(field, std::to_string(chatId).c_str(), CURL_ZERO_TERMINATED);

    field = curl_mime_addpart(form);
    curl_mime_name(field, "voice");
    curl_mime_filedata(field, filePath.c_str());

    std::string responseBody;
    curl_easy_setopt(curl, CURLOPT_URL, endpoint("sendVoice").c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t sz, size_t nm, std::string* s) -> size_t {
        s->append(static_cast<char*>(ptr), sz * nm);
        return sz * nm;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    CURLcode code = curl_easy_perform(curl);
    curl_mime_free(form);
    curl_easy_cleanup(curl);
    return (code == CURLE_OK);
}
