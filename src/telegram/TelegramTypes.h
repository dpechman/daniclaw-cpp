#pragma once
#include <string>
#include <vector>
#include <cstdint>

/**
 * Tipos do Telegram Bot API.
 */
namespace Telegram {

struct User {
    int64_t     id{0};
    std::string firstName;
    std::string lastName;
    std::string username;
};

struct Document {
    std::string fileId;
    std::string fileName;
    std::string mimeType;
    int64_t     fileSize{0};
};

struct Voice {
    std::string fileId;
    int         duration{0};
};

struct Audio {
    std::string fileId;
    int         duration{0};
};

struct Message {
    int64_t     messageId{0};
    int64_t     chatId{0};
    User        from;
    std::string text;
    std::string caption;
    Document    document;
    Voice       voice;
    Audio       audio;
    bool        hasText{false};
    bool        hasDocument{false};
    bool        hasVoice{false};
    bool        hasAudio{false};
};

struct Update {
    int64_t updateId{0};
    Message message;
    bool    hasMessage{false};
};

} // namespace Telegram
