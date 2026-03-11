#pragma once
#include <string>
#include <map>

/**
 * Utilitários gerais de uso interno.
 */
namespace Utils {

/// Gera um UUID v4 sem dependências externas
std::string generateUUID();

/// sleep cross-platform
void sleepMs(unsigned int ms);

/// Remove caracteres Markdown de um texto (para TTS)
std::string stripMarkdown(const std::string& text);

/// Formata timestamp ISO 8601 UTC atual
std::string nowISO();

/// Retorna ISO 8601 UTC daqui a N minutos
std::string nowPlusMinutes(int minutes);

// ── Timezone ─────────────────────────────────────────────────

/// Retorna o offset de fuso horário em horas (lê TZ_OVERRIDE_FILE ou TIMEZONE_OFFSET_HOURS do .env)
int timezoneOffsetHours();

/// Define o override de timezone em runtime (persiste em TZ_OVERRIDE_FILE)
void setTimezoneOverride(int offsetHours);

/// Horário local atual formatado (ISO 8601 com offset, ex: 2026-03-11T10:30:00-03:00)
std::string nowLocalISO();

/// Horário local daqui a N minutos (mesmo formato)
std::string nowPlusMinutesLocal(int minutes);

} // namespace Utils
