// peer_crypto — the primitives LAN pairing and its signed requests are made of, over libcrypto:
// random bytes, SHA-256, HMAC-SHA-256, HKDF-SHA-256, a constant-time comparison and an ephemeral
// X25519 key pair.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

struct evp_pkey_st;

namespace opensu::lan::crypto {

using Bytes = std::vector<std::uint8_t>;

/// An X25519 key, and every key pairing derives, is this long.
inline constexpr std::size_t keyBytes = 32;
/// A request's or a pairing's nonce is this long.
inline constexpr std::size_t nonceBytes = 16;

/// `count` bytes from the system's generator; throws `std::runtime_error` when it has none.
[[nodiscard]] Bytes randomBytes(std::size_t count);

[[nodiscard]] Bytes sha256(std::span<const std::uint8_t> data);
[[nodiscard]] Bytes sha256(std::string_view text);

[[nodiscard]] Bytes hmacSha256(std::span<const std::uint8_t> key, std::string_view message);

/// RFC 5869 with SHA-256: `length` bytes of key from `secret`, `salt` and `info`.
[[nodiscard]] Bytes hkdfSha256(std::span<const std::uint8_t> secret,
                               std::span<const std::uint8_t> salt, std::string_view info,
                               std::size_t length);

/// Whether `a` and `b` hold the same bytes, in time that does not depend on where they differ.
[[nodiscard]] bool sameBytes(std::span<const std::uint8_t> a,
                             std::span<const std::uint8_t> b) noexcept;

/// `bytes` as the characters of a string, for hashing a composed message.
[[nodiscard]] std::string_view asText(std::span<const std::uint8_t> bytes) noexcept;

/// A fresh X25519 key pair, used for one pairing and dropped.
class KeyPair {
  public:
    /// Throws `std::runtime_error` when libcrypto cannot make one.
    KeyPair();
    ~KeyPair();
    KeyPair(const KeyPair&) = delete;
    KeyPair& operator=(const KeyPair&) = delete;
    KeyPair(KeyPair&&) noexcept;
    KeyPair& operator=(KeyPair&&) noexcept;

    [[nodiscard]] const Bytes& publicKey() const noexcept {
        return public_;
    }
    /// The secret shared with the holder of `peerPublic`; nothing when it is not an X25519 key or
    /// makes the all-zero secret.
    [[nodiscard]] std::optional<Bytes> agree(std::span<const std::uint8_t> peerPublic) const;

  private:
    struct Free {
        void operator()(evp_pkey_st* key) const noexcept;
    };
    std::unique_ptr<evp_pkey_st, Free> key_;
    Bytes public_;
};

} // namespace opensu::lan::crypto
