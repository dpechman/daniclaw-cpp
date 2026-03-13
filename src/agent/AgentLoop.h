#pragma once
#include "../providers/ILlmProvider.h"
#include "../tools/ToolRegistry.h"
#include <string>
#include <memory>
#include <optional>
#include <map>
#include <functional>

struct AgentLoopResult {
    std::string answer;
    bool        isAudio{false};
};

/**
 * Motor de raciocínio ReAct: Thought → Action → Observation → Answer
 */
class AgentLoop {
public:
    AgentLoop(std::shared_ptr<ILlmProvider> provider,
              std::shared_ptr<ToolRegistry> registry);

    // Callback chamado imediatamente antes de cada execucao de tool.
    using StepCallback = std::function<void(const std::string& toolName, int iteration)>;
    void setStepCallback(StepCallback cb) { stepCallback_ = std::move(cb); }

    AgentLoopResult run(
        const std::vector<LlmMessage>& messages,
        const std::string& systemPrompt,
        bool requiresAudio = false
    );

private:
    std::shared_ptr<ILlmProvider> provider_;
    std::shared_ptr<ToolRegistry> registry_;
    int maxIterations_;
    StepCallback stepCallback_;

public:
    struct ToolCall {
        std::string name;
        std::map<std::string, std::string> arguments;
    };

private:

    std::optional<ToolCall> parseToolCall(const std::string& response);
};
