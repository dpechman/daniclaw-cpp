#pragma once
#include <string>
#include <sqlite3.h>

/**
 * Singleton de conexão SQLite com WAL ativado.
 */
class Database {
public:
    static Database& getInstance() {
        static Database instance;
        return instance;
    }

    void initialize(const std::string& path = "");
    sqlite3* getDb();
    ~Database();

private:
    Database() = default;
    sqlite3* db_{nullptr};
    void createTables();
};
