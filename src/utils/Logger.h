#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

/**
 * Logger wrapper em torno do spdlog.
 * Singleton com nível configurável.
 */
class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    template<typename... Args>
    void info(const std::string& fmt, Args&&... args) {
        logger_->info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(const std::string& fmt, Args&&... args) {
        logger_->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const std::string& fmt, Args&&... args) {
        logger_->error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(const std::string& fmt, Args&&... args) {
        logger_->debug(fmt, std::forward<Args>(args)...);
    }

private:
    Logger() {
        logger_ = spdlog::stdout_color_mt("daniclaw");
        logger_->set_pattern("[%Y-%m-%dT%H:%M:%S.%e] [%^%l%$] %v");
        logger_->set_level(spdlog::level::debug);
    }
    std::shared_ptr<spdlog::logger> logger_;
};

inline Logger& log() { return Logger::getInstance(); }
