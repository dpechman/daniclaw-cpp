#pragma once
#include "BaseTool.h"

class WebSearchTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"pesquisar_internet"};
    std::string desc_{
        "Pesquisa na internet usando o Google via Serper.dev e retorna os resultados mais relevantes. "
        "Args obrigatórios: query (texto da busca). "
        "Opcional: num (quantidade de resultados, padrão 5). "
        "Use para buscar informações atuais, notícias, fatos, preços, etc."
    };
};
