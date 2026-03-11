#include "DeepSeekProvider.h"
#include "../utils/HttpClient.h"
#include "../utils/Logger.h"
#include "../config/Config.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

DeepSeekProvider::DeepSeekProvider() {
    apiKey_  = cfg().get("DEEPSEEK_API_KEY");
    model_   = cfg().get("DEEPSEEK_MODEL", "deepseek-chat");
    baseUrl_ = cfg().get("DEEPSEEK_BASE_URL", "https://api.deepseek.com/v1");

    if (apiKey_.empty() || apiKey_ == "sua_chave_deepseek_aqui") {
        throw std::runtime_error("DEEPSEEK_API_KEY não configurada no .env");
    }
}

std::string DeepSeekProvider::chat(const std::vector<LlmMessage>& messages,
                                    const std::string& systemPrompt) {
    json payload = {{"model", model_}, {"messages", json::array()}};

    if (!systemPrompt.empty()) {
        payload["messages"].push_back({{"role", "system"}, {"content", systemPrompt}});
    }

    for (const auto& m : messages) {
        std::string role = (m.role == "tool") ? "user" : m.role;
        payload["messages"].push_back({{"role", role}, {"content", m.content}});
    }

    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + apiKey_}
    };

    log().debug("[DeepSeekProvider] Enviando {} msgs para {}", messages.size(), model_);
    auto res = HttpClient::post(baseUrl_ + "/chat/completions", payload.dump(), headers);

    if (!res.ok()) {
        throw std::runtime_error("DeepSeek API error " + std::to_string(res.statusCode) + ": " + res.body);
    }

    try {
        auto j = json::parse(res.body);
        return j["choices"][0]["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        throw std::runtime_error("Erro ao parsear resposta do DeepSeek: " + std::string(e.what()));
    }
}
