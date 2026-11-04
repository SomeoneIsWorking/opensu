// request_signature — how a paired guest's request to a host, and the host's answer, are
// authenticated with the secret their pairing left them: HMAC-SHA-256 over the method, target,
// time, a fresh nonce and the body's SHA-256, and over the nonce, status and body for the answer.
// A host takes a request once, within `clockSlack` of its own clock (`ReplayGuard`).
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

namespace opensu::lan {

/// The headers a signed request and its answer carry.
inline constexpr std::string_view peerHeader = "X-OpenSU-Peer";
inline constexpr std::string_view timeHeader = "X-OpenSU-Time";
inline constexpr std::string_view nonceHeader = "X-OpenSU-Nonce";
inline constexpr std::string_view signatureHeader = "X-OpenSU-Signature";

/// How far a request's time may be from the host's clock.
inline constexpr std::chrono::seconds clockSlack{120};

/// What a request's signature covers.
struct RequestFacts {
    std::string_view method;
    std::string_view target;
    std::int64_t time{0};
    std::string_view nonce;
    std::string_view body;
};

[[nodiscard]] std::string requestSignature(std::span<const std::uint8_t> secret,
                                           const RequestFacts& facts);

[[nodiscard]] std::string answerSignature(std::span<const std::uint8_t> secret,
                                          std::string_view nonce, int status,
                                          std::string_view body);

/// Whether `given` is the signature `expected`, compared in constant time.
[[nodiscard]] bool signatureMatches(std::string_view expected, std::string_view given) noexcept;

/// Seconds since the Unix epoch, the time requests carry.
[[nodiscard]] std::int64_t unixSeconds(std::chrono::system_clock::time_point at);

/// The nonces each peer used within the slack, so a request heard on the LAN cannot be sent again.
class ReplayGuard {
  public:
    /// Whether `peer`'s request at `time` with `nonce` is new and on time at `now`; remembers it.
    [[nodiscard]] bool admit(const std::string& peer, std::string_view nonce, std::int64_t time,
                             std::int64_t now);

  private:
    std::mutex mutex_;
    /// Each "peer nonce" heard, with its time.
    std::map<std::string, std::int64_t, std::less<>> heard_;
};

} // namespace opensu::lan
