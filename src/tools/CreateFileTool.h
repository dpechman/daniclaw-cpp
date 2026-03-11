#pragma once
#include "BaseTool.h"

class CreateFileTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"criar_arquivo"};
    std::string desc_{"Cria um arquivo local com o conteúdo fornecido. Args: filepath, content"};
};
