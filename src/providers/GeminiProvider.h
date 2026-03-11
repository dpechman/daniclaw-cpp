#pragma once
#include "ILlmProvider.h"
#include <string>

class GeminiProvider : public ILlmProvider {
public:
    GeminiProvider();
    const std::string& name() const override { return name_; }
    std::string chat(const std::vector<LlmMessage>& messages,
                     const std::string& systemPrompt = "") override;
private:
    std::string name_{"gemini"};
    std::string apiKey_;
    std::string model_;
};
