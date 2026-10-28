#include "web_client.hpp"

#include <fstream>
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

using Easy = std::unique_ptr<CURL, EasyDeleter>;

/// A request handle that trusts `trust`; null when libcurl cannot start one.
Easy newEasy(const TrustStore& trust) {
    Easy handle{curl_easy_init()};
    if (handle != nullptr && !trust.pem().empty()) {
        curl_blob blob{const_cast<char*>(trust.pem().data()), trust.pem().size(), CURL_BLOB_NOCOPY};
        curl_easy_setopt(handle.get(), CURLOPT_CAINFO_BLOB, &blob);
    }
    return handle;
}

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

std::size_t write(char* data, std::size_t size, std::size_t count, void* file) {
    auto& out = *static_cast<std::ofstream*>(file);
    out.write(data, static_cast<std::streamsize>(size * count));
    return out ? size * count : 0;
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

WebClient::BodyFacts factsOf(CURL* curl) {
    curl_off_t modified = -1;
    curl_off_t length = -1;
    curl_easy_getinfo(curl, CURLINFO_FILETIME_T, &modified);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
    return WebClient::BodyFacts{static_cast<std::int64_t>(modified),
                                static_cast<std::int64_t>(length)};
}

} // namespace

std::string originOf(std::string_view url) {
    const std::size_t scheme = url.find("://");
    if (scheme == std::string_view::npos) {
        return {};
    }
    const std::size_t path = url.find('/', scheme + 3);
    return std::string{url.substr(0, path == std::string_view::npos ? url.size() : path)} + "/";
}

std::string percentEncode(std::string_view text) {
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string out;
    for (const char c : text) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (plain) {
            out.push_back(c);
            continue;
        }
        const auto byte = static_cast<unsigned char>(c);
        out.push_back('%');
        out.push_back(hex[byte >> 4U]);
        out.push_back(hex[byte & 0x0FU]);
    }
    return out;
}

WebClient::WebClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

WebClient::~WebClient() {
    curl_global_cleanup();
}

std::optional<std::string> WebClient::get(const std::string& url, std::string& error,
                                          const std::vector<std::string>& headers) const {
    return fetch(url, error, headers);
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
        const Easy handle = newEasy(trust_);
        if (handle == nullptr) {
            error = "libcurl could not start a request";
            return std::nullopt;
        }
        CappedBody body;
        body.cap = guard.maxBody;
        CURL* curl = handle.get();
        curl_easy_setopt(curl, CURLOPT_URL, next.c_str());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeoutSeconds);
        if (guard.progress) {
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, stallBytesPerSecond);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, stallSeconds);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, report);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &guard.progress);
        } else {
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
        }
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
        if (result == CURLE_ABORTED_BY_CALLBACK) {
            error = "cancelled";
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

std::optional<WebClient::BodyFacts> WebClient::head(const std::string& url, const Refusal& refusal,
                                                    std::string& error) const {
    std::string next = url;
    for (long hop = 0; hop <= redirectLimit; ++hop) {
        if (std::string refused = refusal(next); !refused.empty()) {
            error = std::move(refused);
            return std::nullopt;
        }
        const Easy handle = newEasy(trust_);
        if (handle == nullptr) {
            error = "libcurl could not start a request";
            return std::nullopt;
        }
        CURL* curl = handle.get();
        curl_easy_setopt(curl, CURLOPT_URL, next.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "opensu");
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        const CURLcode result = curl_easy_perform(curl);
        if (result != CURLE_OK) {
            error = curl_easy_strerror(result);
            return std::nullopt;
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
            return std::nullopt;
        }
        return factsOf(curl);
    }
    error = "more than " + std::to_string(redirectLimit) + " redirects";
    return std::nullopt;
}

bool WebClient::download(const std::string& url, const std::filesystem::path& file,
                         const Progress& progress, std::string& error, const Refusal& refusal,
                         BodyFacts* facts) const {
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
        const Easy handle = newEasy(trust_);
        if (handle == nullptr) {
            error = "libcurl could not start a request";
            return false;
        }
        // Each hop starts the file afresh, so a redirect's body never stays in it.
        std::ofstream out{file, std::ios::binary | std::ios::trunc};
        if (!out) {
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
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, report);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
        curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
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
        if (!out.flush()) {
            error = "cannot write " + file.string();
            return false;
        }
        if (facts != nullptr) {
            *facts = factsOf(curl);
        }
        return true;
    }
    error = "more than " + std::to_string(redirectLimit) + " redirects";
    return false;
}

std::optional<std::string> WebClient::fetch(const std::string& url, std::string& error,
                                            const std::vector<std::string>& headers) const {
    const Easy handle = newEasy(trust_);
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
    if (status != httpOk) {
        error = "HTTP " + std::to_string(status);
        return std::nullopt;
    }
    return body;
}

} // namespace opensu::net
