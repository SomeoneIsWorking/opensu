// request_signature — how a paired guest's request to a host, and the host's answer, are protected
// with the secret their pairing left them. The request is signed: HMAC-SHA-256 over the method,
// target, time, a fresh nonce and the sealed body's SHA-256; a host takes it once, within
// `clockSlack` of its own clock (`ReplayGuard`). Its body and the answer's are sealed with
// ChaCha20-Poly1305 under keys HKDF-SHA-256 derives from the secret for that request's nonce, one
// per direction, so each key seals one message and an answer opens only for the request it
// answers.
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
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

/// Which way a sealed body goes.
enum class Direction : std::uint8_t { Request, Answer };

/// `plain` sealed for the request `facts` names (its `body` is not read), going `direction`: a
/// random ChaCha20-Poly1305 nonce, then the ciphertext and tag under a key only that request and
/// direction use. An answer's `status` is authenticated with it.
[[nodiscard]] std::string sealBody(std::span<const std::uint8_t> secret, const RequestFacts& facts,
                                   Direction direction, int status, std::string_view plain);

/// What `sealed` holds; nothing when it was not sealed with `secret` for that request, direction
/// and status.
[[nodiscard]] std::optional<std::string> openBody(std::span<const std::uint8_t> secret,
                                                  const RequestFacts& facts, Direction direction,
                                                  int status, std::string_view sealed);

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
