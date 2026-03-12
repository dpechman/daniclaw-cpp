#include "WebSearchTool.h"
#include "../config/Config.h"
#include "../utils/HttpClient.h"
#include "../utils/Logger.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

ToolResult WebSearchTool::execute(const std::map<std::string, std::string>& args) {
    auto qIt = args.find("query");
    if (qIt == args.end() || qIt->second.empty()) {
        return {"", "Argumento obrigatório faltando: query."};
    }

    std::string apiKey = cfg().get("SERPER_API_KEY");
    if (apiKey.empty()) {
        return {"", "SERPER_API_KEY não configurada no .env."};
    }

    int num = 5;
    auto nIt = args.find("num");
    if (nIt != args.end()) {
        try { num = std::stoi(nIt->second); } catch (...) {}
    }
    if (num < 1) num = 1;
    if (num > 10) num = 10;

    json payload = {
        {"q",   qIt->second},
        {"num", num},
        {"gl",  "br"},
        {"hl",  "pt"}
    };

    std::map<std::string, std::string> headers = {
        {"X-API-KEY", apiKey}
    };

    log().info("[WebSearchTool] Buscando: {}", qIt->second);
    auto res = HttpClient::post("https://google.serper.dev/search", payload.dump(), headers);

    if (!res.ok()) {
        log().warn("[WebSearchTool] Erro HTTP {}: {}", res.statusCode, res.body);
        return {"", "Erro ao buscar na internet (HTTP " + std::to_string(res.statusCode) + ")."};
    }

    try {
        auto j = json::parse(res.body);

        std::ostringstream out;
        out << "Resultados para \"" << qIt->second << "\":\n\n";

        // Resposta direta (answer box)
        if (j.contains("answerBox")) {
            auto& ab = j["answerBox"];
            std::string title   = ab.value("title", "");
            std::string answer  = ab.value("answer", ab.value("snippet", ""));
            if (!answer.empty()) {
                out << "📌 Resposta direta: " << answer << "\n\n";
            }
        }

        // Knowledge graph
        if (j.contains("knowledgeGraph")) {
            auto& kg = j["knowledgeGraph"];
            std::string desc = kg.value("description", "");
            if (!desc.empty()) {
                out << "📚 " << desc << "\n\n";
            }
        }

        // Resultados orgânicos
        if (j.contains("organic")) {
            int count = 0;
            for (const auto& item : j["organic"]) {
                if (count >= num) break;
                std::string title   = item.value("title", "");
                std::string link    = item.value("link", "");
                std::string snippet = item.value("snippet", "");
                out << (count + 1) << ". **" << title << "**\n";
                if (!snippet.empty()) out << "   " << snippet << "\n";
                if (!link.empty())    out << "   🔗 " << link << "\n";
                out << "\n";
                ++count;
            }
        }

        std::string result = out.str();
        if (result.empty()) result = "Nenhum resultado encontrado.";
        return {result, ""};

    } catch (const std::exception& e) {
        return {"", std::string("Erro ao processar resposta da busca: ") + e.what()};
    }
}
