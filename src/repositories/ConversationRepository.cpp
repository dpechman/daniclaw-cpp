#include "ConversationRepository.h"
#include "../utils/Utils.h"
#include "../config/Config.h"
#include <sqlite3.h>
#include <optional>

std::optional<Conversation> ConversationRepository::findByUserId(const std::string& userId) {
    sqlite3* db = Database::getInstance().getDb();
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, user_id, provider, created_at, updated_at "
                      "FROM conversations WHERE user_id = ? "
                      "ORDER BY updated_at DESC LIMIT 1";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);

    std::optional<Conversation> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Conversation c;
        c.id        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        c.userId    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        c.provider  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        c.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        c.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        result = c;
    }
    sqlite3_finalize(stmt);
    return result;
}

Conversation ConversationRepository::create(const std::string& userId, const std::string& provider) {
    sqlite3* db = Database::getInstance().getDb();
    Conversation c;
    c.id        = Utils::generateUUID();
    c.userId    = userId;
    c.provider  = provider.empty() ? cfg().get("LLM_PROVIDER", "groq") : provider;
    c.createdAt = Utils::nowISO();
    c.updatedAt = c.createdAt;

    const char* sql = "INSERT INTO conversations (id, user_id, provider, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, c.id.c_str(),        -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, c.userId.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, c.provider.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, c.createdAt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, c.updatedAt.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return c;
}

void ConversationRepository::touch(const std::string& conversationId) {
    sqlite3* db = Database::getInstance().getDb();
    std::string now = Utils::nowISO();
    const char* sql = "UPDATE conversations SET updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, now.c_str(),            -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, conversationId.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

Conversation ConversationRepository::findOrCreate(const std::string& userId, const std::string& provider) {
    auto existing = findByUserId(userId);
    if (existing.has_value()) return existing.value();
    return create(userId, provider);
}
