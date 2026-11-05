// web_client — HTTPS GETs over libcurl: artwork, the stores' account APIs and emulator downloads;
// plain HTTP GETs and POSTs to other openSUs on the LAN. Every request is made on a stop token,
// which ends it the moment its stop is requested (`Stopped`), whatever the transfer waits for.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "result.hpp"
#include "trust_store.hpp"

namespace opensu::net {

/// `scheme://authority/` of `url`, or empty when it has none.
[[nodiscard]] std::string originOf(std::string_view url);

/// `text` with everything but RFC 3986's unreserved characters percent-encoded.
[[nodiscard]] std::string percentEncode(std::string_view text);

class WebClient {
  public:
    /// Initialises libcurl for the process; keep one alive while any request runs.
    WebClient();
    ~WebClient();
    WebClient(const WebClient&) = delete;
    WebClient& operator=(const WebClient&) = delete;

    /// The body of a 200 response, following redirects, with each of `headers` ("Name: value")
    /// sent; a failure says why ("HTTP 404", or libcurl's reason).
    [[nodiscard]] Result<std::string> get(const std::stop_token& stop, const std::string& url,
                                          const std::vector<std::string>& headers = {}) const;

    /// A response of any status.
    struct Answer {
        long status{0};
        std::string body;
        /// The last hop's header lines as name and value, in the order they came.
        std::vector<std::pair<std::string, std::string>> headers{};

        /// The first header named `name`, compared without case; empty when there is none.
        [[nodiscard]] std::string header(std::string_view name) const;
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

    /// What a request sends beyond a GET: its method (POST with a body, else GET, when empty),
    /// its body, and Basic credentials when `user` is set.
    struct Sent {
        std::string method;
        std::optional<std::string> body;
        std::string user;
        std::string password;
    };

    /// The response to `sent` of `url` with `headers` sent, following redirects that `guard`
    /// allows. A failure when a hop is refused, the body is over `guard.maxBody` or the transfer
    /// fails; stopped when `stop` is requested or `guard.progress` cancels it.
    [[nodiscard]] Result<Answer> request(const std::stop_token& stop, const std::string& url,
                                         const std::vector<std::string>& headers,
                                         const Guard& guard, const Sent& sent = {}) const;

    /// What a 200 response says of its body.
    struct BodyFacts {
        /// Last-Modified as Unix time; -1 when the server sent none.
        std::int64_t modified{-1};
        /// Content-Length; -1 when the server sent none.
        std::int64_t length{-1};
    };

    /// The facts of a HEAD of `url`, following redirects that `refusal` allows; a failure unless
    /// the answer is 200.
    [[nodiscard]] Result<BodyFacts> head(const std::stop_token& stop, const std::string& url,
                                         const Refusal& refusal) const;

    /// Streams the body of a 200 response to `file`, following redirects that `refusal` allows,
    /// or without one, only those within the URL's own scheme, and gives the response's facts.
    /// `progress` is called as bytes arrive and cancels the transfer by returning false, as `stop`
    /// does. A body shorter than its Content-Length fails. Unless it succeeds `file` may hold a
    /// partial body, which the caller removes.
    [[nodiscard]] Result<BodyFacts> download(const std::stop_token& stop, const std::string& url,
                                             const std::filesystem::path& file,
                                             const Progress& progress,
                                             const Refusal& refusal = {}) const;

  private:
    TrustStore trust_;
};

} // namespace opensu::net
