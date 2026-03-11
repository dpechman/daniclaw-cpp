#pragma once
#include <string>
#include <vector>

struct LlmMessage {
    std::string role;    // "user" | "assistant" | "system" | "tool"
    std::string content;
};

/**
 * Interface base para todos os provedores de LLM.
 */
class ILlmProvider {
public:
    virtual ~ILlmProvider() = default;
    virtual const std::string& name() const = 0;
    virtual std::string chat(
        const std::vector<LlmMessage>& messages,
        const std::string& systemPrompt = ""
    ) = 0;
};
