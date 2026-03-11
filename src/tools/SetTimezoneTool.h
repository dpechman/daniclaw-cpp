#pragma once
#include "BaseTool.h"

class SetTimezoneTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"definir_timezone"};
    std::string desc_{
        "Define o fuso horário local do assistente. "
        "Use quando o usuário informar que está em outro país/cidade ou pedir para alterar o horário. "
        "Args: offset_hours (número inteiro, ex: -3 para BRT, 1 para CET, -5 para EST, 9 para JST). "
        "O novo fuso persiste entre sessões."
    };
};
