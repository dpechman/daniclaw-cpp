#pragma once
#include "BaseTool.h"

class ShellTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"executar_comando"};
    std::string desc_{
        "Executa um comando shell na máquina e retorna stdout + stderr. "
        "Args: command (string com o comando a executar). "
        "Use para verificar processos, espaço em disco, logs, status de serviços, etc."
    };

    bool isAllowed(const std::string& command) const;
};
