#include "HttpClient.h"
#include <curl/curl.h>
#include <fstream>
#include <stdexcept>

size_t HttpClient::writeCallback(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

size_t HttpClient::fileWriteCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    auto* fp = static_cast<FILE*>(stream);
    return fwrite(ptr, size, nmemb, fp);
}

HttpClient::Response HttpClient::post(
    const std::string& url,
    const std::string& jsonBody,
    const std::map<std::string, std::string>& headers
) {
    Response res;
    CURL* curl = curl_easy_init();
    if (!curl) return res;

    struct curl_slist* curlHeaders = nullptr;
    curlHeaders = curl_slist_append(curlHeaders, "Content-Type: application/json");
    for (const auto& [key, val] : headers) {
        curlHeaders = curl_slist_append(curlHeaders, (key + ": " + val).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        res.statusCode = static_cast<int>(http_code);
    }

    curl_slist_free_all(curlHeaders);
    curl_easy_cleanup(curl);
    return res;
}

HttpClient::Response HttpClient::get(
    const std::string& url,
    const std::map<std::string, std::string>& headers
) {
    Response res;
    CURL* curl = curl_easy_init();
    if (!curl) return res;

    struct curl_slist* curlHeaders = nullptr;
    for (const auto& [key, val] : headers) {
        curlHeaders = curl_slist_append(curlHeaders, (key + ": " + val).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (curlHeaders) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    // Long-polling support
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);

    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        res.statusCode = static_cast<int>(http_code);
    }

    if (curlHeaders) curl_slist_free_all(curlHeaders);
    curl_easy_cleanup(curl);
    return res;
}

bool HttpClient::downloadFile(const std::string& url, const std::string& destPath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    FILE* fp = fopen(destPath.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fileWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode code = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    return (code == CURLE_OK);
}
