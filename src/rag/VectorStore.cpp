#include "VectorStore.h"
#include "../database/Database.h"
#include "../utils/Logger.h"
#include <sqlite3.h>
#include <stdexcept>
#include <cstring>

// ── helpers ───────────────────────────────────────────────────────────────────

static std::vector<uint8_t> floatsToBlob(const std::vector<float>& v) {
    std::vector<uint8_t> blob(v.size() * sizeof(float));
    std::memcpy(blob.data(), v.data(), blob.size());
    return blob;
}

static sqlite3* db() { return Database::getInstance().getDb(); }

// ── VectorStore ───────────────────────────────────────────────────────────────

void VectorStore::initialize(int dims) {
    // A extensão sqlite-vec já foi registrada via sqlite3_auto_extension
    // antes de Database::initialize() ser chamado em main.cpp.

    // Tabela de metadados
    const char* metaSql = R"(
        CREATE TABLE IF NOT EXISTS rag_chunks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            source      TEXT    NOT NULL,
            chunk_index INTEGER NOT NULL,
            content     TEXT    NOT NULL,
            created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
        );
        CREATE INDEX IF NOT EXISTS idx_rag_chunks_source ON rag_chunks(source);
    )";

    char* err = nullptr;
    if (sqlite3_exec(db(), metaSql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("[VectorStore] Erro ao criar rag_chunks: " + msg);
    }

    // Tabela vetorial (vec0) — dimensão fixada em tempo de criação
    std::string vecSql =
        "CREATE VIRTUAL TABLE IF NOT EXISTS rag_vectors USING vec0("
        "  id INTEGER PRIMARY KEY,"
        "  embedding FLOAT[" + std::to_string(dims) + "]"
        ");";

    if (sqlite3_exec(db(), vecSql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("[VectorStore] Erro ao criar rag_vectors: " + msg);
    }

    log().debug("[VectorStore] Inicializado com {} dimensões.", dims);
}

int64_t VectorStore::insert(const std::string& source,
                             int                chunkIndex,
                             const std::string& content,
                             const std::vector<float>& embedding) {
    // 1. Insere metadados e obtém rowid
    const char* metaSql =
        "INSERT INTO rag_chunks(source, chunk_index, content) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db(), metaSql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("[VectorStore] Prepare insert metadata falhou.");

    sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, chunkIndex);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("[VectorStore] Insert metadata falhou.");
    }
    sqlite3_finalize(stmt);
    int64_t rowid = sqlite3_last_insert_rowid(db());

    // 2. Insere embedding com o mesmo id
    auto blob = floatsToBlob(embedding);
    const char* vecSql = "INSERT INTO rag_vectors(id, embedding) VALUES (?, ?)";
    if (sqlite3_prepare_v2(db(), vecSql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("[VectorStore] Prepare insert vector falhou.");

    sqlite3_bind_int64(stmt, 1, rowid);
    sqlite3_bind_blob (stmt, 2, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("[VectorStore] Insert vector falhou.");
    }
    sqlite3_finalize(stmt);

    return rowid;
}

std::vector<RagChunk> VectorStore::search(const std::vector<float>& queryEmbedding, int topK) {
    auto blob = floatsToBlob(queryEmbedding);

    const char* sql = R"(
        SELECT c.id, c.source, c.chunk_index, c.content
        FROM rag_vectors v
        JOIN rag_chunks c ON c.id = v.id
        WHERE v.embedding MATCH ?
          AND k = ?
        ORDER BY v.distance
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("[VectorStore] Prepare search falhou: " +
                                 std::string(sqlite3_errmsg(db())));

    sqlite3_bind_blob (stmt, 1, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int  (stmt, 2, topK);

    std::vector<RagChunk> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RagChunk chunk;
        chunk.id         = sqlite3_column_int64(stmt, 0);
        chunk.source     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        chunk.chunkIndex = sqlite3_column_int(stmt, 2);
        chunk.content    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        results.push_back(std::move(chunk));
    }
    sqlite3_finalize(stmt);
    return results;
}

void VectorStore::deleteSource(const std::string& source) {
    // Remove vetores primeiro (FK manual via subquery)
    const char* delVec = R"(
        DELETE FROM rag_vectors WHERE id IN
        (SELECT id FROM rag_chunks WHERE source = ?)
    )";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db(), delVec, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char* delMeta = "DELETE FROM rag_chunks WHERE source = ?";
    sqlite3_prepare_v2(db(), delMeta, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    log().info("[VectorStore] Fonte removida: {}", source);
}

bool VectorStore::sourceExists(const std::string& source) {
    const char* sql = "SELECT COUNT(*) FROM rag_chunks WHERE source = ?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
}
