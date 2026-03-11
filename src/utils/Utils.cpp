#include "Utils.h"
#include "../config/Config.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <regex>
#include <fstream>
#include <atomic>
#include <climits>

namespace Utils {

std::string generateUUID() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t hi = dis(gen);
    uint64_t lo = dis(gen);

    // Versão 4
    hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Variante RFC 4122
    lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << ((hi >> 32) & 0xFFFFFFFF) << '-';
    oss << std::setw(4) << ((hi >> 16) & 0xFFFF)     << '-';
    oss << std::setw(4) << (hi & 0xFFFF)              << '-';
    oss << std::setw(4) << ((lo >> 48) & 0xFFFF)     << '-';
    oss << std::setw(12) << (lo & 0xFFFFFFFFFFFFULL);
    return oss.str();
}

void sleepMs(unsigned int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

std::string stripMarkdown(const std::string& text) {
    // Remove cabeçalhos, bold, italic, code, blockquote
    static const std::regex mdRe(R"([*_`#>~|])");
    return std::regex_replace(text, mdRe, "");
}

std::string nowISO() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    struct tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string nowPlusMinutes(int minutes) {
    auto future = std::chrono::system_clock::now() + std::chrono::minutes(minutes);
    auto t      = std::chrono::system_clock::to_time_t(future);
    std::ostringstream oss;
    struct tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ── Timezone ─────────────────────────────────────────────────────────────────

static const std::string TZ_OVERRIDE_FILE = "./data/.tz_override";

// Cache atômico: INT_MIN = não inicializado
static std::atomic<int> s_tzCache{INT_MIN};

int timezoneOffsetHours() {
    int cached = s_tzCache.load();
    if (cached != INT_MIN) return cached;

    // 1. Tenta arquivo de override em runtime
    {
        std::ifstream f(TZ_OVERRIDE_FILE);
        if (f.is_open()) {
            int v = 0;
            if (f >> v) {
                s_tzCache.store(v);
                return v;
            }
        }
    }

    // 2. Fallback: .env / variável de ambiente
    int v = cfg().getInt("TIMEZONE_OFFSET_HOURS", -3);
    s_tzCache.store(v);
    return v;
}

void setTimezoneOverride(int offsetHours) {
    s_tzCache.store(offsetHours);
    std::ofstream f(TZ_OVERRIDE_FILE, std::ios::trunc);
    if (f.is_open()) f << offsetHours;
}

static std::string formatWithOffset(std::time_t t, int offsetHours) {
    // Aplica offset manualmente sobre o tempo UTC
    std::time_t local_t = t + static_cast<std::time_t>(offsetHours * 3600);
    struct tm tm_buf{};
    gmtime_r(&local_t, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");

    // Sufixo de offset: +HH:MM ou -HH:MM
    int absH = std::abs(offsetHours);
    oss << (offsetHours >= 0 ? "+" : "-")
        << std::setw(2) << std::setfill('0') << absH
        << ":00";
    return oss.str();
}

std::string nowLocalISO() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    return formatWithOffset(t, timezoneOffsetHours());
}

std::string nowPlusMinutesLocal(int minutes) {
    auto future = std::chrono::system_clock::now() + std::chrono::minutes(minutes);
    auto t      = std::chrono::system_clock::to_time_t(future);
    return formatWithOffset(t, timezoneOffsetHours());
}

} // namespace Utils
