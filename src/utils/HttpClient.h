#pragma once
#include <string>
#include <map>
#include <vector>

/**
 * Cliente HTTP leve baseado em libcurl.
 * Suporta GET, POST e download de arquivos.
 */
class HttpClient {
public:
    struct Response {
        int         statusCode{0};
        std::string body;
        bool ok() const { return statusCode >= 200 && statusCode < 300; }
    };

    static Response post(
        const std::string& url,
        const std::string& jsonBody,
        const std::map<std::string, std::string>& headers = {}
    );

    static Response get(
        const std::string& url,
        const std::map<std::string, std::string>& headers = {}
    );

    static bool downloadFile(
        const std::string& url,
        const std::string& destPath
    );

private:
    static size_t writeCallback(void* ptr, size_t size, size_t nmemb, std::string* data);
    static size_t fileWriteCallback(void* ptr, size_t size, size_t nmemb, void* stream);
};
