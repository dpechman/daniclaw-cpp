#include "ReminderManager.h"
#include "../database/Database.h"
#include "../utils/Logger.h"
#include "../utils/Utils.h"
#include <sqlite3.h>

ReminderManager::ReminderManager(TelegramBot& bot) : bot_(bot) {}

ReminderManager::~ReminderManager() {
    stop();
}

void ReminderManager::start(int checkIntervalMs) {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&ReminderManager::loop, this, checkIntervalMs);
    log().info("⏰ ReminderManager iniciado.");
}

void ReminderManager::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ReminderManager::loop(int intervalMs) {
    while (running_) {
        try {
            checkAndFire();
        } catch (const std::exception& e) {
            log().error("[ReminderManager] Erro no loop de verificação: {}", e.what());
        }
        Utils::sleepMs(intervalMs);
    }
}

void ReminderManager::checkAndFire() {
    sqlite3* db = Database::getInstance().getDb();
    std::string now = Utils::nowISO();

    const char* sql = "SELECT id, chat_id, message FROM reminders WHERE completed = 0 AND remind_at <= ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_STATIC);

    struct Reminder { std::string id, chatId, msg; };
    std::vector<Reminder> dueReminders;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        dueReminders.push_back({
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))
        });
    }
    sqlite3_finalize(stmt);

    for (const auto& r : dueReminders) {
        log().info("⏰ Disparando lembrete para {}: {}", r.chatId, r.msg);
        
        // Envia via telegram. Ignora falhas por enquanto (pode ser que bloqueou o bot)
        int64_t cid = 0;
        try { cid = std::stoll(r.chatId); } catch (...) {}
        
        if (cid != 0) {
            bot_.sendMessage(cid, "⏰ *Lembrete:* " + r.msg);
        }

        // Marca como completo
        const char* upd = "UPDATE reminders SET completed = 1 WHERE id = ?";
        sqlite3_stmt* updStmt;
        if (sqlite3_prepare_v2(db, upd, -1, &updStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(updStmt, 1, r.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(updStmt);
            sqlite3_finalize(updStmt);
        }
    }
}

bool ReminderManager::schedule(const std::string& chatId, const std::string& message, const std::string& remindAtISO) {
    sqlite3* db = Database::getInstance().getDb();
    std::string id = Utils::generateUUID();
    std::string createdAt = Utils::nowISO();

    const char* sql = "INSERT INTO reminders (id, chat_id, message, remind_at, completed, created_at) "
                      "VALUES (?, ?, ?, ?, 0, ?)";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, id.c_str(),           -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, chatId.c_str(),       -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message.c_str(),      -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, remindAtISO.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, createdAt.c_str(),    -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
