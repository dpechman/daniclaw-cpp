#pragma once
#include "BaseTool.h"

class ReadFileTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"ler_arquivo"};
    std::string desc_{"Lê e retorna o conteúdo de um arquivo local. Args: filepath"};
};
