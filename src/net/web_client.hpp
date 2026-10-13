// web_client — HTTPS GETs over libcurl: artwork, the stores' account APIs and emulator downloads.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace opensu::net {

class WebClient {
  public:
    /// Initialises libcurl for the process; keep one alive while any request runs.
    WebClient();
    ~WebClient();
    WebClient(const WebClient&) = delete;
    WebClient& operator=(const WebClient&) = delete;

    /// The body of a 200 response, following redirects, with each of `headers` ("Name: value")
    /// sent. Nothing otherwise; `error` says why ("HTTP 404" or libcurl's reason).
    [[nodiscard]] std::optional<std::string>
    get(const std::string& url, std::string& error,
        const std::vector<std::string>& headers = {}) const;

    /// The bytes `first` to `last` inclusive of a URL, from a 206 response. A 200 is an error: the
    /// server ignored the range.
    [[nodiscard]] std::optional<std::string> getRange(const std::string& url, std::uint64_t first,
                                                      std::uint64_t last, std::string& error) const;

    /// Bytes received and the size the server announced (0 when unknown); returns false to cancel.
    using Progress = std::function<bool(std::uint64_t received, std::uint64_t total)>;

    /// Streams the body of a 200 response to `file`, following redirects only within the URL's
    /// own scheme. `progress` is called as bytes arrive and cancels the transfer by returning
    /// false. On failure `error` says why ("cancelled", "HTTP 404" or libcurl's reason) and
    /// `file` may hold a partial body, which the caller removes.
    [[nodiscard]] bool download(const std::string& url, const std::filesystem::path& file,
                                const Progress& progress, std::string& error) const;

  private:
    /// One GET following redirects, answered by `expected`; `range` is "first-last" or empty.
    [[nodiscard]] std::optional<std::string> fetch(const std::string& url, const std::string& range,
                                                   long expected, std::string& error,
                                                   const std::vector<std::string>& headers) const;
};

} // namespace opensu::net
