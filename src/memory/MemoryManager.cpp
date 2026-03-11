#include "MemoryManager.h"
#include "../config/Config.h"

MemoryManager::MemoryManager()
    : windowSize_(cfg().getInt("MEMORY_WINDOW_SIZE", 20))
{}

void MemoryManager::saveUserMessage(const std::string& userId, const std::string& content) {
    auto conv = convRepo_.findOrCreate(userId);
    msgRepo_.save(conv.id, "user", content);
    convRepo_.touch(conv.id);
}

void MemoryManager::saveAssistantMessage(const std::string& userId, const std::string& content) {
    auto conv = convRepo_.findOrCreate(userId);
    msgRepo_.save(conv.id, "assistant", content);
    convRepo_.touch(conv.id);
}

void MemoryManager::saveToolMessage(const std::string& userId, const std::string& content) {
    auto conv = convRepo_.findOrCreate(userId);
    msgRepo_.save(conv.id, "tool", content);
}

std::vector<LlmMessage> MemoryManager::getContext(const std::string& userId) {
    auto conv = convRepo_.findOrCreate(userId);
    return msgRepo_.findLastN(conv.id, windowSize_);
}
