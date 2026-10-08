// web_client — HTTPS GETs over libcurl, for artwork from the internet.
#pragma once

#include <optional>
#include <string>

namespace iideck::artwork {

class WebClient {
  public:
    /// Initialises libcurl for the process; keep one alive while any request runs.
    WebClient();
    ~WebClient();
    WebClient(const WebClient&) = delete;
    WebClient& operator=(const WebClient&) = delete;

    /// The body of a 200 response, following redirects. Nothing otherwise; `error` says why
    /// ("HTTP 404" or libcurl's reason).
    [[nodiscard]] std::optional<std::string> get(const std::string& url, std::string& error) const;
};

} // namespace iideck::artwork
