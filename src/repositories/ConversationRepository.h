#pragma once
#include "../database/Database.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

struct Conversation {
    std::string id;
    std::string userId;
    std::string provider;
    std::string createdAt;
    std::string updatedAt;
};

class ConversationRepository {
public:
    std::optional<Conversation> findByUserId(const std::string& userId);
    Conversation create(const std::string& userId, const std::string& provider = "groq");
    void touch(const std::string& conversationId);
    Conversation findOrCreate(const std::string& userId, const std::string& provider = "");
};
