#include "ReadFileTool.h"
#include <filesystem>
#include <fstream>
#include <sstream>

ToolResult ReadFileTool::execute(const std::map<std::string, std::string>& args) {
    auto it = args.find("filepath");
    if (it == args.end()) return {"", "filepath é obrigatório."};

    std::string filepath = it->second;
    if (!std::filesystem::exists(filepath)) {
        return {"", "Arquivo não encontrado: " + filepath};
    }

    try {
        std::ifstream f(filepath);
        if (!f) return {"", "Não foi possível abrir: " + filepath};
        std::ostringstream ss;
        ss << f.rdbuf();
        return {ss.str(), ""};
    } catch (const std::exception& e) {
        return {"", std::string("Erro ao ler arquivo: ") + e.what()};
    }
}
