#include "Embedder.h"
#include "../config/Config.h"
#include "../utils/HttpClient.h"
#include "../utils/Logger.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

Embedder::Embedder() {
    apiKey_  = cfg().get("OPENAI_API_KEY");
    model_   = cfg().get("EMBEDDING_MODEL", "text-embedding-3-small");
    baseUrl_ = cfg().get("OPENAI_BASE_URL", "https://api.openai.com/v1");
    dims_    = cfg().getInt("EMBEDDING_DIMS", 1536);

    if (apiKey_.empty()) {
        throw std::runtime_error("[Embedder] OPENAI_API_KEY não configurada.");
    }
}

std::vector<float> Embedder::embed(const std::string& text) {
    json payload = {
        {"model", model_},
        {"input", text}
    };

    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + apiKey_}
    };

    auto res = HttpClient::post(baseUrl_ + "/embeddings", payload.dump(), headers);

    if (!res.ok()) {
        log().warn("[Embedder] Erro HTTP {}: {}", res.statusCode, res.body.substr(0, 200));
        throw std::runtime_error("Falha ao gerar embedding (HTTP " + std::to_string(res.statusCode) + ").");
    }

    try {
        auto j = json::parse(res.body);
        auto& vals = j["data"][0]["embedding"];
        std::vector<float> vec;
        vec.reserve(vals.size());
        for (const auto& v : vals) {
            vec.push_back(v.get<float>());
        }
        return vec;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Erro ao parsear embedding: ") + e.what());
    }
}
