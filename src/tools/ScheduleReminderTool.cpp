#include "ScheduleReminderTool.h"
#include "../reminders/ReminderManager.h"
#include "../utils/Logger.h"
#include "../utils/Utils.h"

ToolResult ScheduleReminderTool::execute(const std::map<std::string, std::string>& args) {
    auto cidIt  = args.find("chat_id");
    auto msgIt  = args.find("message");

    if (cidIt == args.end() || msgIt == args.end()) {
        return {"", "Argumentos obrigatórios faltando: chat_id, message."};
    }

    std::string remindAt;

    // Preferência: delay_minutes (tempo relativo)
    auto delayIt = args.find("delay_minutes");
    if (delayIt != args.end()) {
        try {
            int minutes = std::stoi(delayIt->second);
            if (minutes <= 0) return {"", "delay_minutes deve ser um número positivo."};
            remindAt = Utils::nowPlusMinutes(minutes);
        } catch (...) {
            return {"", "delay_minutes inválido: " + delayIt->second};
        }
    } else {
        // Fallback: data_iso_utc absoluta
        auto timeIt = args.find("data_iso_utc");
        if (timeIt == args.end()) {
            return {"", "Forneça delay_minutes ou data_iso_utc."};
        }
        remindAt = timeIt->second;
    }

    bool ok = ReminderManager::schedule(cidIt->second, msgIt->second, remindAt);

    if (ok) {
        log().debug("[ScheduleReminderTool] Agendou '{}' para {}", msgIt->second, remindAt);
        return {"✅ Lembrete salvo com sucesso para " + remindAt, ""};
    } else {
        return {"", "Erro interno ao tentar salvar o lembrete no banco de dados."};
    }
}
