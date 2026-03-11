#pragma once
#include "../providers/ILlmProvider.h"
#include <string>
#include <vector>

struct DbMessage {
    std::string id;
    std::string conversationId;
    std::string role;
    std::string content;
    std::string createdAt;
};

class MessageRepository {
public:
    DbMessage save(const std::string& conversationId,
                   const std::string& role,
                   const std::string& content);

    std::vector<LlmMessage> findLastN(const std::string& conversationId, int n);
};
