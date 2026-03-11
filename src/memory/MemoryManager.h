#pragma once
#include "../providers/ILlmProvider.h"
#include "../repositories/ConversationRepository.h"
#include "../repositories/MessageRepository.h"
#include <string>
#include <vector>

/**
 * Facade de memória persistente.
 * Centraliza acesso aos repositórios e controla janela de contexto.
 */
class MemoryManager {
public:
    MemoryManager();
    void saveUserMessage(const std::string& userId, const std::string& content);
    void saveAssistantMessage(const std::string& userId, const std::string& content);
    void saveToolMessage(const std::string& userId, const std::string& content);
    std::vector<LlmMessage> getContext(const std::string& userId);

private:
    ConversationRepository convRepo_;
    MessageRepository      msgRepo_;
    int windowSize_;
};
