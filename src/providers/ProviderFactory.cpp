#include "ProviderFactory.h"
#include "GeminiProvider.h"
#include "GroqProvider.h"
#include "DeepSeekProvider.h"
#include "OpenAIProvider.h"
#include "../config/Config.h"
#include "../utils/Logger.h"

std::shared_ptr<ILlmProvider> ProviderFactory::create(const std::string& providerName) {
    std::string name = providerName.empty()
        ? cfg().get("LLM_PROVIDER", "groq")
        : providerName;

    try {
        if (name == "gemini")   return std::make_shared<GeminiProvider>();
        if (name == "groq")     return std::make_shared<GroqProvider>();
        if (name == "deepseek") return std::make_shared<DeepSeekProvider>();
        if (name == "openai")   return std::make_shared<OpenAIProvider>();
        // Fallback
        log().warn("[ProviderFactory] Provedor '{}' desconhecido, usando groq.", name);
        return std::make_shared<GroqProvider>();
    } catch (const std::exception& e) {
        log().error("[ProviderFactory] Falha ao criar '{}': {}", name, e.what());
        throw;
    }
}
