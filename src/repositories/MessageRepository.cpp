#include "MessageRepository.h"
#include "../database/Database.h"
#include "../utils/Utils.h"
#include <sqlite3.h>
#include <algorithm>
#include <regex>

// Sanitiza null bytes que quebram o SQLite
static std::string sanitize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) if (c != '\0') out += c;
    return out;
}

DbMessage MessageRepository::save(const std::string& conversationId,
                                   const std::string& role,
                                   const std::string& content) {
    sqlite3* db = Database::getInstance().getDb();
    DbMessage msg;
    msg.id             = Utils::generateUUID();
    msg.conversationId = conversationId;
    msg.role           = role;
    msg.content        = sanitize(content);
    msg.createdAt      = Utils::nowISO();

    const char* sql = "INSERT INTO messages (id, conversation_id, role, content, created_at) "
                      "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, msg.id.c_str(),             -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, msg.conversationId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, msg.role.c_str(),           -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, msg.content.c_str(),        -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, msg.createdAt.c_str(),      -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return msg;
}

std::vector<LlmMessage> MessageRepository::findLastN(const std::string& conversationId, int n) {
    sqlite3* db = Database::getInstance().getDb();
    const char* sql =
        "SELECT role, content FROM ("
        "  SELECT role, content, created_at FROM messages"
        "  WHERE conversation_id = ? ORDER BY created_at DESC LIMIT ?"
        ") ORDER BY created_at ASC";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, conversationId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, n);

    std::vector<LlmMessage> msgs;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LlmMessage m;
        m.role    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msgs.push_back(std::move(m));
    }
    sqlite3_finalize(stmt);
    return msgs;
}
