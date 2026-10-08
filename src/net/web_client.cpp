#include "web_client.hpp"

#include <memory>

#include <curl/curl.h>

namespace iideck::net {
namespace {

struct HeaderListDeleter {
    void operator()(curl_slist* list) const noexcept {
        curl_slist_free_all(list);
    }
};

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
constexpr long httpPartial = 206;

} // namespace

WebClient::WebClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

WebClient::~WebClient() {
    curl_global_cleanup();
}

std::optional<std::string> WebClient::get(const std::string& url, std::string& error,
                                          const std::vector<std::string>& headers) const {
    return fetch(url, {}, httpOk, error, headers);
}

std::optional<std::string> WebClient::getRange(const std::string& url, std::uint64_t first,
                                               std::uint64_t last, std::string& error) const {
    return fetch(url, std::to_string(first) + "-" + std::to_string(last), httpPartial, error, {});
}

std::optional<std::string> WebClient::fetch(const std::string& url, const std::string& range,
                                            long expected, std::string& error,
                                            const std::vector<std::string>& headers) const {
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
    if (!range.empty()) {
        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    }
    curl_slist* list = nullptr;
    for (const std::string& header : headers) {
        list = curl_slist_append(list, header.c_str());
    }
    const std::unique_ptr<curl_slist, HeaderListDeleter> sent{list};
    if (list != nullptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    }
    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        return std::nullopt;
    }
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != expected) {
        error = "HTTP " + std::to_string(status);
        return std::nullopt;
    }
    return body;
}

} // namespace iideck::net
