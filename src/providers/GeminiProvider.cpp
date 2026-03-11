#include "GeminiProvider.h"
#include "../utils/HttpClient.h"
#include "../utils/Logger.h"
#include "../config/Config.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

GeminiProvider::GeminiProvider() {
    apiKey_ = cfg().get("GEMINI_API_KEY");
    model_  = cfg().get("GEMINI_MODEL", "gemini-1.5-flash");

    if (apiKey_.empty() || apiKey_ == "sua_chave_aqui") {
        throw std::runtime_error("GEMINI_API_KEY não configurada no .env");
    }
}

std::string GeminiProvider::chat(const std::vector<LlmMessage>& messages,
                                  const std::string& systemPrompt) {
    // Gemini usa formato diferente do OpenAI
    json payload;

    if (!systemPrompt.empty()) {
        payload["system_instruction"] = {{"parts", {{{"text", systemPrompt}}}}};
    }

    payload["contents"] = json::array();
    for (const auto& m : messages) {
        std::string role = (m.role == "assistant") ? "model" : "user";
        payload["contents"].push_back({
            {"role", role},
            {"parts", {{{"text", m.content}}}}
        });
    }

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" +
                      model_ + ":generateContent?key=" + apiKey_;

    log().debug("[GeminiProvider] Enviando {} msgs para {}", messages.size(), model_);
    auto res = HttpClient::post(url, payload.dump());

    if (!res.ok()) {
        throw std::runtime_error("Gemini API error " + std::to_string(res.statusCode) + ": " + res.body);
    }

    try {
        auto j = json::parse(res.body);
        return j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
    } catch (const std::exception& e) {
        throw std::runtime_error("Erro ao parsear resposta do Gemini: " + std::string(e.what()));
    }
}
