#include "Database.h"
#include "../config/Config.h"
#include "../utils/Logger.h"
#include <filesystem>
#include <stdexcept>

void Database::initialize(const std::string& path) {
    std::string dbPath = path.empty()
        ? cfg().get("DB_PATH", "./data/daniclaw.db")
        : path;

    // Cria diretório se não existir
    std::filesystem::path p(dbPath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Falha ao abrir SQLite: " + std::string(sqlite3_errmsg(db_)));
    }

    // WAL para melhor concorrência leitura/escrita
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=OFF;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    createTables();
    log().info("💾 SQLite conectado em: {}", dbPath);
}

sqlite3* Database::getDb() {
    if (!db_) throw std::runtime_error("Database não inicializado. Chame initialize() primeiro.");
    return db_;
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::createTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS conversations (
            id         TEXT PRIMARY KEY,
            user_id    TEXT NOT NULL,
            provider   TEXT NOT NULL DEFAULT 'groq',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS messages (
            id              TEXT PRIMARY KEY,
            conversation_id TEXT NOT NULL,
            role            TEXT NOT NULL CHECK(role IN ('user','assistant','system','tool')),
            content         TEXT NOT NULL,
            created_at      TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_messages_conv_id ON messages(conversation_id);
        CREATE INDEX IF NOT EXISTS idx_conversations_user_id ON conversations(user_id);
        CREATE TABLE IF NOT EXISTS reminders (
            id         TEXT PRIMARY KEY,
            chat_id    TEXT NOT NULL,
            message    TEXT NOT NULL,
            remind_at  TEXT NOT NULL,
            completed  INTEGER DEFAULT 0,
            created_at TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_reminders_remind_at ON reminders(remind_at);
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "desconhecido";
        sqlite3_free(errMsg);
        throw std::runtime_error("Erro ao criar tabelas: " + err);
    }
}
