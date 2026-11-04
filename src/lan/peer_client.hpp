// peer_client — this openSU asking another one on the LAN, over HTTP to the address and port its
// announcement resolved to: the open pairing calls, and calls signed with the pairing's secret,
// their bodies and answers sealed with it (`request_signature`), whose answers count only when
// they open. Nothing is followed off that address.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "net/web_client.hpp"

namespace opensu::lan {

/// Where an openSU on the LAN listens.
struct Endpoint {
    std::string address;
    std::uint16_t port{0};
};

class PeerClient {
  public:
    PeerClient(const net::WebClient& web, Endpoint endpoint)
        : web_{web}, endpoint_{std::move(endpoint)} {
    }

    /// A POST of `body` to `target`, signed by no one; nothing with `error` when it did not
    /// arrive or the answer is over `largest` bytes.
    [[nodiscard]] std::optional<net::WebClient::Answer> post(std::string_view target,
                                                             const std::string& body,
                                                             std::size_t largest,
                                                             std::string& error) const;

    /// Who is asking and with what: this openSU's peer id and the pairing's secret.
    struct Signer {
        std::string_view selfId;
        std::span<const std::uint8_t> secret;
    };

    /// `method` (GET, or POST with `body` sealed) of `target`, signed by `signer`; the answer as
    /// it opens. Nothing with `error` when it did not arrive, the answer is over `largest` bytes or
    /// was not sealed by the host with the same secret for this request.
    [[nodiscard]] std::optional<net::WebClient::Answer>
    call(std::string_view method, std::string_view target, const std::string& body,
         const Signer& signer, std::size_t largest, std::string& error) const;

  private:
    [[nodiscard]] std::optional<net::WebClient::Answer>
    send(std::string_view target, const std::vector<std::string>& headers,
         const std::optional<std::string>& post, std::size_t largest, std::string& error) const;

    const net::WebClient& web_;
    Endpoint endpoint_;
};

} // namespace opensu::lan
