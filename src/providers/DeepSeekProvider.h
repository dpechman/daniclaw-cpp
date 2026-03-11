#pragma once
#include "ILlmProvider.h"
#include <string>

class DeepSeekProvider : public ILlmProvider {
public:
    DeepSeekProvider();
    const std::string& name() const override { return name_; }
    std::string chat(const std::vector<LlmMessage>& messages,
                     const std::string& systemPrompt = "") override;
private:
    std::string name_{"deepseek"};
    std::string apiKey_;
    std::string model_;
    std::string baseUrl_;
};
