#pragma once
#include <string>
#include <vector>

struct RagChunk {
    int64_t     id;
    std::string source;
    int         chunkIndex;
    std::string content;
};

/**
 * Armazena e busca chunks de documentos usando sqlite-vec para busca por similaridade.
 */
class VectorStore {
public:
    /// Inicializa as tabelas RAG e carrega a extensão sqlite-vec.
    /// Deve ser chamado após Database::initialize().
    static void initialize(int dims = 1536);

    /// Insere um chunk e seu embedding. Retorna o ID gerado.
    static int64_t insert(const std::string& source,
                          int                chunkIndex,
                          const std::string& content,
                          const std::vector<float>& embedding);

    /// Busca os K chunks mais similares ao vetor de query.
    static std::vector<RagChunk> search(const std::vector<float>& queryEmbedding,
                                        int topK = 5);

    /// Remove todos os chunks de uma fonte (para re-indexar).
    static void deleteSource(const std::string& source);

    /// Verifica se uma fonte já foi indexada.
    static bool sourceExists(const std::string& source);
};
