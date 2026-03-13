#include "SemanticSearchTool.h"
#include "../rag/Embedder.h"
#include "../rag/VectorStore.h"
#include "../utils/Logger.h"
#include <sstream>

ToolResult SemanticSearchTool::execute(const std::map<std::string, std::string>& args) {
    auto qIt = args.find("query");
    if (qIt == args.end() || qIt->second.empty()) {
        return {"", "Argumento obrigatório faltando: query."};
    }

    int topK = 5;
    auto kIt = args.find("top_k");
    if (kIt != args.end()) {
        try { topK = std::stoi(kIt->second); } catch (...) {}
    }
    if (topK < 1) topK = 1;
    if (topK > 20) topK = 20;

    try {
        Embedder embedder;
        auto queryVec = embedder.embed(qIt->second);
        auto results  = VectorStore::search(queryVec, topK);

        if (results.empty()) {
            return {"Nenhum documento relevante encontrado na base de conhecimento.", ""};
        }

        std::ostringstream out;
        out << "📚 Resultados da base de conhecimento para: \"" << qIt->second << "\"\n\n";
        for (int i = 0; i < static_cast<int>(results.size()); ++i) {
            out << (i + 1) << ". [" << results[i].source
                << " #" << results[i].chunkIndex << "]\n"
                << results[i].content << "\n\n";
        }
        return {out.str(), ""};

    } catch (const std::exception& e) {
        log().warn("[SemanticSearchTool] Erro: {}", e.what());
        return {"", std::string("Erro na busca semântica: ") + e.what()};
    }
}
