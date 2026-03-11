#include "ShellTool.h"
#include "../config/Config.h"
#include "../utils/Logger.h"
#include <array>
#include <sstream>
#include <stdexcept>
#include <cstdio>

bool ShellTool::isAllowed(const std::string& command) const {
    std::string whitelist = cfg().get("SHELL_ALLOWED_COMMANDS");
    if (whitelist.empty()) return true; // sem restrição

    // Extrai o primeiro token do comando (o binário em si)
    std::string binary;
    std::istringstream iss(command);
    iss >> binary;
    // Remove caminho se houver (ex: /usr/bin/ls -> ls)
    auto slash = binary.rfind('/');
    if (slash != std::string::npos) binary = binary.substr(slash + 1);

    // Verifica se está na whitelist (separada por vírgula)
    std::istringstream wss(whitelist);
    std::string token;
    while (std::getline(wss, token, ',')) {
        // trim
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (token == binary) return true;
    }
    return false;
}

ToolResult ShellTool::execute(const std::map<std::string, std::string>& args) {
    auto it = args.find("command");
    if (it == args.end() || it->second.empty()) {
        return {"", "Argumento obrigatório faltando: command."};
    }

    const std::string& cmd = it->second;

    if (!isAllowed(cmd)) {
        log().warn("[ShellTool] Comando bloqueado pela whitelist: {}", cmd);
        return {"", "Comando não permitido. Verifique SHELL_ALLOWED_COMMANDS no .env."};
    }

    log().info("[ShellTool] Executando: {}", cmd);

    // Redireciona stderr para stdout
    std::string fullCmd = "(" + cmd + ") 2>&1";

    std::array<char, 4096> buf{};
    std::ostringstream output;
    int exitCode = 0;

    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        return {"", "Falha ao abrir pipe para execução do comando."};
    }

    while (fgets(buf.data(), buf.size(), pipe)) {
        output << buf.data();
        // Limita output a 8KB para não estourar contexto do LLM
        if (output.tellp() > 8192) {
            output << "\n[... saída truncada em 8KB ...]";
            break;
        }
    }

    exitCode = pclose(pipe);
    int code = WEXITSTATUS(exitCode);

    std::string result = output.str();
    if (result.empty()) result = "(sem saída)";

    log().debug("[ShellTool] Exit {}: {}...", code,
        result.substr(0, std::min<size_t>(result.size(), 80)));

    return {"```\n" + result + "\n```\nExit code: " + std::to_string(code), ""};
}
