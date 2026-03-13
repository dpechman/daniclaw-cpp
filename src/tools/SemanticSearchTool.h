#pragma once
#include "BaseTool.h"

class SemanticSearchTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"busca_semantica"};
    std::string desc_{
        "Busca documentos indexados na base de conhecimento (RAG) por similaridade semântica. "
        "Use para responder perguntas com base em manuais, documentações e arquivos indexados. "
        "Args obrigatórios: query (pergunta ou trecho relevante). "
        "Opcional: top_k (quantidade de resultados, padrão 5)."
    };
};
