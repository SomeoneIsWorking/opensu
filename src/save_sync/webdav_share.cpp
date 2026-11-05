#include "webdav_share.hpp"

#include <utility>

#include "save_bundle.hpp"
#include "secrets/secret.hpp"

namespace opensu::save_sync {
namespace {

constexpr long created = 201;
constexpr long noContent = 204;
constexpr long ok = 200;
constexpr long notFound = 404;
constexpr long notAllowed = 405;
constexpr long conflict = 409;

bool succeeded(long status) {
    return status == ok || status == created || status == noContent;
}

/// HTTPS anywhere, HTTP only to this machine.
std::string refusalOf(const std::string& url) {
    if (url.starts_with("https://") || url.starts_with("http://127.0.0.1") ||
        url.starts_with("http://localhost") || url.starts_with("http://[::1]")) {
        return {};
    }
    return "a WebDAV share is reached over HTTPS only";
}

} // namespace

WebDavShare::WebDavShare(const net::WebClient& web, std::string url, std::string user,
                         std::string password)
    : web_{web}, url_{std::move(url)}, user_{std::move(user)}, password_{std::move(password)} {
    if (!url_.ends_with('/')) {
        url_ += '/';
    }
}

WebDavShare::~WebDavShare() {
    secrets::wipe(password_);
}

std::string WebDavShare::where() const {
    return url_;
}

bool WebDavShare::reachable() const {
    return refusalOf(url_).empty();
}

std::string WebDavShare::urlOf(const std::string& name) const {
    std::string url = url_;
    std::size_t start = 0;
    while (start <= name.size()) {
        const std::size_t end = std::min(name.find('/', start), name.size());
        url += net::percentEncode(std::string_view{name}.substr(start, end - start));
        if (end < name.size()) {
            url += '/';
        }
        start = end + 1;
    }
    return url;
}

std::optional<net::WebClient::Answer> WebDavShare::send(const std::string& method,
                                                        const std::string& name,
                                                        std::optional<std::string> body,
                                                        const std::vector<std::string>& headers,
                                                        std::string& error) const {
    const net::WebClient::Guard guard{
        .maxBody = largestBundle * 2, .refusal = refusalOf, .progress = {}};
    net::Result<net::WebClient::Answer> answer = web_.request(
        std::stop_token{}, urlOf(name), headers, guard,
        net::WebClient::Sent{
            .method = method, .body = std::move(body), .user = user_, .password = password_});
    if (!answer) {
        error = answer.stopped() ? "stopped" : answer.error();
        return std::nullopt;
    }
    return *std::move(answer);
}

SaveShare::Read WebDavShare::read(const std::string& name) const {
    std::string error;
    std::optional<net::WebClient::Answer> answer = send("GET", name, std::nullopt, {}, error);
    if (!answer) {
        return Read{.kind = Read::Kind::Failed, .bytes = std::move(error)};
    }
    if (answer->status == notFound) {
        return Read{.kind = Read::Kind::Missing, .bytes = {}};
    }
    if (answer->status != ok) {
        return Read{.kind = Read::Kind::Failed, .bytes = "HTTP " + std::to_string(answer->status)};
    }
    return Read{.kind = Read::Kind::Found, .bytes = std::move(answer->body)};
}

bool WebDavShare::makeFolders(const std::string& name, std::string& error) const {
    for (std::size_t slash = name.find('/'); slash != std::string::npos;
         slash = name.find('/', slash + 1)) {
        const std::optional<net::WebClient::Answer> answer =
            send("MKCOL", name.substr(0, slash + 1), std::nullopt, {}, error);
        if (!answer) {
            return false;
        }
        // 405: it is there already.
        if (!succeeded(answer->status) && answer->status != notAllowed) {
            error =
                name.substr(0, slash) + " cannot be made: HTTP " + std::to_string(answer->status);
            return false;
        }
    }
    return true;
}

bool WebDavShare::write(const std::string& name, const std::string& bytes,
                        std::string& error) const {
    const std::string part = name + ".part";
    std::optional<net::WebClient::Answer> answer = send("PUT", part, bytes, {}, error);
    if (answer && answer->status == conflict) {
        if (!makeFolders(name, error)) {
            return false;
        }
        answer = send("PUT", part, bytes, {}, error);
    }
    if (!answer) {
        return false;
    }
    if (!succeeded(answer->status)) {
        error = "HTTP " + std::to_string(answer->status) + " for " + part;
        return false;
    }
    answer =
        send("MOVE", part, std::nullopt, {"Destination: " + urlOf(name), "Overwrite: T"}, error);
    if (!answer) {
        return false;
    }
    if (!succeeded(answer->status)) {
        error = "HTTP " + std::to_string(answer->status) + " moving " + part;
        return false;
    }
    return true;
}

} // namespace opensu::save_sync
