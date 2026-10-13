#include "web_client.hpp"

#include <cstdio>
#include <memory>

#include <curl/curl.h>

namespace opensu::net {
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

/// A body that stops taking bytes past its cap.
struct CappedBody {
    std::string bytes;
    std::size_t cap{0};
    bool over{false};
};

std::size_t appendCapped(char* data, std::size_t size, std::size_t count, void* sink) {
    auto& body = *static_cast<CappedBody*>(sink);
    if (body.bytes.size() + size * count > body.cap) {
        body.over = true;
        return 0;
    }
    body.bytes.append(data, size * count);
    return size * count;
}

struct FileCloser {
    void operator()(std::FILE* file) const noexcept {
        std::fclose(file);
    }
};

std::size_t write(char* data, std::size_t size, std::size_t count, void* file) {
    return std::fwrite(data, size, count, static_cast<std::FILE*>(file)) * size;
}

int report(void* progress, curl_off_t total, curl_off_t received, curl_off_t, curl_off_t) {
    const auto& callback = *static_cast<const WebClient::Progress*>(progress);
    return callback(static_cast<std::uint64_t>(received), static_cast<std::uint64_t>(total)) ? 0
                                                                                             : 1;
}

constexpr long connectTimeoutSeconds = 10;
/// A download slower than this many bytes a second for `stallSeconds` has stalled.
constexpr long stallBytesPerSecond = 1024;
constexpr long stallSeconds = 30;
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

std::optional<WebClient::Answer> WebClient::request(const std::string& url,
                                                    const std::vector<std::string>& headers,
                                                    const Guard& guard, std::string& error) const {
    curl_slist* list = nullptr;
    for (const std::string& header : headers) {
        list = curl_slist_append(list, header.c_str());
    }
    const std::unique_ptr<curl_slist, HeaderListDeleter> sent{list};
    std::string next = url;
    for (long hop = 0; hop <= redirectLimit; ++hop) {
        if (std::string refused = guard.refusal(next); !refused.empty()) {
            error = std::move(refused);
            return std::nullopt;
        }
        const std::unique_ptr<CURL, EasyDeleter> handle{curl_easy_init()};
        if (handle == nullptr) {
            error = "libcurl could not start a request";
            return std::nullopt;
        }
        CappedBody body;
        body.cap = guard.maxBody;
        CURL* curl = handle.get();
        curl_easy_setopt(curl, CURLOPT_URL, next.c_str());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "opensu");
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendCapped);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        if (list != nullptr) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        }
        const CURLcode result = curl_easy_perform(curl);
        if (body.over) {
            error = "the answer is over " + std::to_string(guard.maxBody) + " bytes";
            return std::nullopt;
        }
        if (result != CURLE_OK) {
            error = curl_easy_strerror(result);
            return std::nullopt;
        }
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        char* location = nullptr;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &location);
        if (location == nullptr) {
            return Answer{status, std::move(body.bytes)};
        }
        next = location;
    }
    error = "more than " + std::to_string(redirectLimit) + " redirects";
    return std::nullopt;
}

bool WebClient::download(const std::string& url, const std::filesystem::path& file,
                         const Progress& progress, std::string& error,
                         const Refusal& refusal) const {
    const std::string scheme = url.substr(0, url.find(':') + 1);
    const Refusal sameScheme = [&scheme](const std::string& next) {
        return next.starts_with(scheme) ? std::string{} : next + " leaves " + scheme;
    };
    const Refusal& allowed = refusal ? refusal : sameScheme;
    std::string next = url;
    for (long hop = 0; hop <= redirectLimit; ++hop) {
        if (std::string refused = allowed(next); !refused.empty()) {
            error = std::move(refused);
            return false;
        }
        const std::unique_ptr<CURL, EasyDeleter> handle{curl_easy_init()};
        if (handle == nullptr) {
            error = "libcurl could not start a request";
            return false;
        }
        // Each hop starts the file afresh, so a redirect's body never stays in it.
        const std::unique_ptr<std::FILE, FileCloser> out{std::fopen(file.c_str(), "wb")};
        if (out == nullptr) {
            error = "cannot write " + file.string();
            return false;
        }
        CURL* curl = handle.get();
        curl_easy_setopt(curl, CURLOPT_URL, next.c_str());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, stallBytesPerSecond);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, stallSeconds);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "opensu");
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out.get());
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, report);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
        const CURLcode result = curl_easy_perform(curl);
        if (result == CURLE_ABORTED_BY_CALLBACK) {
            error = "cancelled";
            return false;
        }
        if (result != CURLE_OK) {
            error = curl_easy_strerror(result);
            return false;
        }
        char* location = nullptr;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &location);
        if (location != nullptr) {
            next = location;
            continue;
        }
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        if (status != httpOk) {
            error = "HTTP " + std::to_string(status);
            return false;
        }
        if (std::fflush(out.get()) != 0) {
            error = "cannot write " + file.string();
            return false;
        }
        return true;
    }
    error = "more than " + std::to_string(redirectLimit) + " redirects";
    return false;
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
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "opensu");
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

} // namespace opensu::net
