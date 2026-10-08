#include "web_client.hpp"

#include <memory>

#include <curl/curl.h>

namespace iideck::artwork {
namespace {

struct EasyDeleter {
    void operator()(CURL* handle) const noexcept {
        curl_easy_cleanup(handle);
    }
};

std::size_t append(char* data, std::size_t size, std::size_t count, void* body) {
    static_cast<std::string*>(body)->append(data, size * count);
    return size * count;
}

constexpr long connectTimeoutSeconds = 10;
constexpr long timeoutSeconds = 30;
constexpr long redirectLimit = 5;
constexpr long httpOk = 200;

} // namespace

WebClient::WebClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

WebClient::~WebClient() {
    curl_global_cleanup();
}

std::optional<std::string> WebClient::get(const std::string& url, std::string& error) const {
    const std::unique_ptr<CURL, EasyDeleter> handle{curl_easy_init()};
    if (handle == nullptr) {
        error = "libcurl could not start a request";
        return std::nullopt;
    }
    std::string body;
    CURL* curl = handle.get();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, redirectLimit);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "iideck");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        return std::nullopt;
    }
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != httpOk) {
        error = "HTTP " + std::to_string(status);
        return std::nullopt;
    }
    return body;
}

} // namespace iideck::artwork
