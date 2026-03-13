#pragma once
#include "BaseTool.h"

class IndexDocumentTool : public BaseTool {
public:
    const std::string& name() const override        { return name_; }
    const std::string& description() const override { return desc_; }
    ToolResult execute(const std::map<std::string, std::string>& args) override;
private:
    std::string name_{"indexar_documento"};
    std::string desc_{
        "Indexa um arquivo de texto na base de conhecimento RAG para buscas semânticas futuras. "
        "Args obrigatórios: filepath (caminho do arquivo .txt, .md ou similar). "
        "Opcional: chunk_size (tamanho dos trechos em caracteres, padrão 800), "
        "force (reindexar mesmo se já existir, padrão false)."
    };
};
