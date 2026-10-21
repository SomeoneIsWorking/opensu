// web_client — HTTPS GETs over libcurl: artwork, the stores' account APIs and emulator downloads.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::net {

/// `scheme://authority/` of `url`, or empty when it has none.
[[nodiscard]] std::string originOf(std::string_view url);

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

    /// A response of any status.
    struct Answer {
        long status{0};
        std::string body;
    };

    /// Bytes received and the size the server announced (0 when unknown); returns false to cancel.
    using Progress = std::function<bool(std::uint64_t received, std::uint64_t total)>;

    /// Why `url` may not be requested, or empty when it may. Asked for the URL and for every
    /// redirect before it is followed.
    using Refusal = std::function<std::string(const std::string& url)>;

    /// What a guarded request may do: how large a body it takes, and which URLs it may reach.
    /// With `progress` it is told of each hop's bytes as they arrive, may be cancelled, and has
    /// only a stall limit instead of an overall time limit.
    struct Guard {
        std::size_t maxBody{0};
        Refusal refusal;
        Progress progress{};
    };

    /// The response to a GET of `url` with `headers` sent, following redirects that `guard`
    /// allows. Nothing when a hop is refused, the body is over `guard.maxBody`, the transfer
    /// fails or `guard.progress` cancels it; `error` says why ("cancelled" for the last).
    [[nodiscard]] std::optional<Answer> request(const std::string& url,
                                                const std::vector<std::string>& headers,
                                                const Guard& guard, std::string& error) const;

    /// What a 200 response says of its body.
    struct BodyFacts {
        /// Last-Modified as Unix time; -1 when the server sent none.
        std::int64_t modified{-1};
        /// Content-Length; -1 when the server sent none.
        std::int64_t length{-1};
    };

    /// The facts of a HEAD of `url`, following redirects that `refusal` allows. Nothing unless the
    /// answer is 200; `error` says why.
    [[nodiscard]] std::optional<BodyFacts> head(const std::string& url, const Refusal& refusal,
                                                std::string& error) const;

    /// Streams the body of a 200 response to `file`, following redirects that `refusal` allows,
    /// or without one, only those within the URL's own scheme. `progress` is called as bytes
    /// arrive and cancels the transfer by returning false. A body shorter than its Content-Length
    /// fails. With `facts`, the response's facts are put there. On failure `error` says why
    /// ("cancelled", "HTTP 404", the refusal or libcurl's reason) and `file` may hold a partial
    /// body, which the caller removes.
    [[nodiscard]] bool download(const std::string& url, const std::filesystem::path& file,
                                const Progress& progress, std::string& error,
                                const Refusal& refusal = {}, BodyFacts* facts = nullptr) const;

  private:
    /// One GET following redirects, answered by `expected`; `range` is "first-last" or empty.
    [[nodiscard]] std::optional<std::string> fetch(const std::string& url, const std::string& range,
                                                   long expected, std::string& error,
                                                   const std::vector<std::string>& headers) const;
};

} // namespace opensu::net
