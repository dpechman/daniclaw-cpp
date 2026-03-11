#include "CreateFileTool.h"
#include <filesystem>
#include <fstream>

ToolResult CreateFileTool::execute(const std::map<std::string, std::string>& args) {
    auto fpIt = args.find("filepath");
    auto cnIt = args.find("content");
    if (fpIt == args.end()) return {"", "filepath é obrigatório."};

    std::string filepath = fpIt->second;
    std::string content  = (cnIt != args.end()) ? cnIt->second : "";

    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream f(filepath);
        if (!f) return {"", "Não foi possível abrir o arquivo para escrita: " + filepath};
        f << content;
        return {"✅ Arquivo criado: " + filepath, ""};
    } catch (const std::exception& e) {
        return {"", std::string("Erro ao criar arquivo: ") + e.what()};
    }
}
