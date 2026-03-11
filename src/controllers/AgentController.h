#pragma once
#include "../telegram/TelegramBot.h"
#include "../telegram/TelegramTypes.h"
#include "../agent/AgentLoop.h"
#include "../memory/MemoryManager.h"
#include "../skills/SkillLoader.h"
#include "../tools/ToolRegistry.h"
#include "../providers/ProviderFactory.h"
#include <set>
#include <string>
#include <memory>

/**
 * Facade principal do DaniClaw.
 * Registra handlers e orquestra o pipeline completo.
 */
class AgentController {
public:
    explicit AgentController(TelegramBot& bot);
    void registerHandlers();

private:
    TelegramBot&                    bot_;
    std::set<std::string>           allowedUserIds_;
    std::unique_ptr<MemoryManager>  memory_;
    std::shared_ptr<ToolRegistry>   toolRegistry_;
    SkillLoader                     skillLoader_;
    SkillRouter                     skillRouter_;

    bool isAllowed(int64_t userId);
    bool detectAudioRequest(const std::string& text);
    void processInput(int64_t chatId, const std::string& userId,
                      const std::string& text, bool requiresAudio);

    void handleText(const Telegram::Message& msg);
    void handleDocument(const Telegram::Message& msg);
    void handleVoice(const Telegram::Message& msg);
};
