#include "SetTimezoneTool.h"
#include "../utils/Utils.h"
#include "../utils/Logger.h"

ToolResult SetTimezoneTool::execute(const std::map<std::string, std::string>& args) {
    auto it = args.find("offset_hours");
    if (it == args.end()) {
        return {"", "Argumento obrigatório faltando: offset_hours (ex: -3, 0, 1, 9)."};
    }

    int offset = 0;
    try {
        offset = std::stoi(it->second);
    } catch (...) {
        return {"", "offset_hours inválido: '" + it->second + "'. Deve ser um número inteiro."};
    }

    if (offset < -12 || offset > 14) {
        return {"", "offset_hours fora do intervalo válido (-12 a +14)."};
    }

    Utils::setTimezoneOverride(offset);

    std::string sign = (offset >= 0) ? "+" : "";
    std::string msg  = "✅ Fuso horário definido para UTC" + sign + std::to_string(offset) +
                       ". Horário local atual: " + Utils::nowLocalISO();

    log().info("[SetTimezoneTool] Timezone alterado para UTC{}{}", sign, offset);
    return {msg, ""};
}
