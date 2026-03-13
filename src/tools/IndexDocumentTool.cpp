#include "IndexDocumentTool.h"
#include "../rag/Embedder.h"
#include "../rag/VectorStore.h"
#include "../utils/Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>

// Divide texto em chunks com overlap
static std::vector<std::string> chunkText(const std::string& text, int chunkSize, int overlap = 100) {
    std::vector<std::string> chunks;
    if (text.empty()) return chunks;

    int len = static_cast<int>(text.size());
    int step = chunkSize - overlap;
    if (step <= 0) step = chunkSize;

    for (int i = 0; i < len; i += step) {
        // Tenta quebrar no próximo espaço/newline após chunkSize
        int end = std::min(i + chunkSize, len);
        if (end < len) {
            int lookahead = std::min(end + 50, len);
            for (int j = end; j < lookahead; ++j) {
                if (text[j] == '\n' || text[j] == ' ') { end = j + 1; break; }
            }
        }
        std::string chunk = text.substr(i, end - i);
        // Trim
        size_t s = chunk.find_first_not_of(" \t\n\r");
        size_t e = chunk.find_last_not_of(" \t\n\r");
        if (s != std::string::npos) {
            chunks.push_back(chunk.substr(s, e - s + 1));
        }
    }
    return chunks;
}

ToolResult IndexDocumentTool::execute(const std::map<std::string, std::string>& args) {
    auto fpIt = args.find("filepath");
    if (fpIt == args.end() || fpIt->second.empty()) {
        return {"", "Argumento obrigatório faltando: filepath."};
    }

    std::string filepath = fpIt->second;
    if (!std::filesystem::exists(filepath)) {
        return {"", "Arquivo não encontrado: " + filepath};
    }

    int chunkSize = 800;
    auto csIt = args.find("chunk_size");
    if (csIt != args.end()) {
        try { chunkSize = std::stoi(csIt->second); } catch (...) {}
    }
    if (chunkSize < 100) chunkSize = 100;
    if (chunkSize > 4000) chunkSize = 4000;

    bool force = false;
    auto fIt = args.find("force");
    if (fIt != args.end()) force = (fIt->second == "true" || fIt->second == "1");

    // Verifica se já foi indexado
    if (!force && VectorStore::sourceExists(filepath)) {
        return {"ℹ️ Documento já indexado: " + filepath +
                "\nUse force=true para reindexar.", ""};
    }

    // Lê o arquivo
    std::ifstream f(filepath);
    if (!f) return {"", "Não foi possível abrir: " + filepath};
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    if (content.empty()) return {"", "Arquivo vazio: " + filepath};

    // Remove indexação anterior se force
    if (force) VectorStore::deleteSource(filepath);

    // Chunking
    auto chunks = chunkText(content, chunkSize);
    if (chunks.empty()) return {"", "Não foi possível extrair chunks do arquivo."};

    // Embed + insert
    Embedder embedder;
    int indexed = 0;
    std::vector<std::string> errors;

    log().info("[IndexDocumentTool] Indexando {} chunks de '{}'", chunks.size(), filepath);

    for (int i = 0; i < static_cast<int>(chunks.size()); ++i) {
        try {
            auto vec = embedder.embed(chunks[i]);
            VectorStore::insert(filepath, i, chunks[i], vec);
            ++indexed;
        } catch (const std::exception& e) {
            errors.push_back("chunk " + std::to_string(i) + ": " + e.what());
            log().warn("[IndexDocumentTool] Erro no chunk {}: {}", i, e.what());
        }
    }

    std::ostringstream out;
    out << "✅ Indexação concluída: " << filepath << "\n"
        << "   Chunks indexados: " << indexed << "/" << chunks.size() << "\n"
        << "   Tamanho do chunk: " << chunkSize << " caracteres";
    if (!errors.empty()) {
        out << "\n⚠️ Erros em " << errors.size() << " chunks.";
    }
    return {out.str(), ""};
}
