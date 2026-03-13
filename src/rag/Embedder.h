#pragma once
#include <string>
#include <vector>

/**
 * Gera embeddings vetoriais usando a API da OpenAI.
 * Modelo padrão: text-embedding-3-small (1536 dimensões).
 */
class Embedder {
public:
    Embedder();

    /// Retorna vetor float32 com o embedding do texto.
    std::vector<float> embed(const std::string& text);

    int dimensions() const { return dims_; }

private:
    std::string apiKey_;
    std::string model_;
    std::string baseUrl_;
    int         dims_;
};
