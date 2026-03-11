#pragma once
#include "BaseTool.h"

class ScheduleReminderTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"agendar_lembrete"};
    std::string desc_{
        "Agenda um lembrete para envio no futuro. "
        "Args obrigatórios: chat_id, message. "
        "Para tempo relativo use delay_minutes (ex: 5 para daqui a 5 minutos). "
        "Alternativamente use data_iso_utc (ex: 2026-03-10T22:30:00Z). "
        "Prefira delay_minutes quando o usuário especificar um intervalo (ex: 'em 10 minutos')."
    };
};
