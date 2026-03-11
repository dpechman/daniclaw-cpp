#pragma once
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdlib>

/**
 * Singleton para carregar e acessar variáveis de ambiente do .env.
 * Também popula o ambiente do processo via setenv().
 */
class Config {
public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    void load(const std::string& path = ".env") {
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key   = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            env_[key] = value;
            setenv(key.c_str(), value.c_str(), 1);
        }
    }

    std::string get(const std::string& key, const std::string& defaultVal = "") const {
        auto it = env_.find(key);
        if (it != env_.end()) return it->second;
        const char* val = std::getenv(key.c_str());
        return val ? val : defaultVal;
    }

    int getInt(const std::string& key, int defaultVal = 0) const {
        std::string val = get(key);
        if (val.empty()) return defaultVal;
        try { return std::stoi(val); }
        catch (...) { return defaultVal; }
    }

    bool getBool(const std::string& key, bool defaultVal = false) const {
        std::string val = get(key);
        return val.empty() ? defaultVal : (val == "true" || val == "1" || val == "yes");
    }

private:
    Config() = default;
    std::map<std::string, std::string> env_;

    static std::string trim(const std::string& s) {
        const std::string ws = " \t\r\n\"'";
        size_t start = s.find_first_not_of(ws);
        size_t end   = s.find_last_not_of(ws);
        if (start == std::string::npos) return "";
        return s.substr(start, end - start + 1);
    }
};

// Atalho global
inline Config& cfg() { return Config::getInstance(); }
