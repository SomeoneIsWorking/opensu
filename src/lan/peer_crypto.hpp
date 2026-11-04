// peer_crypto — the primitives LAN pairing and its signed, sealed requests are made of, over
// libcrypto: random bytes, SHA-256, HMAC-SHA-256, HKDF-SHA-256, ChaCha20-Poly1305, a constant-time
// comparison and an ephemeral X25519 key pair.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct evp_pkey_st;
struct evp_md_ctx_st;

namespace opensu::lan::crypto {

using Bytes = std::vector<std::uint8_t>;

/// An X25519 key, and every key pairing derives, is this long.
inline constexpr std::size_t keyBytes = 32;
/// A request's or a pairing's nonce is this long.
inline constexpr std::size_t nonceBytes = 16;

/// A ChaCha20-Poly1305 nonce is this long, and its tag, which follows the ciphertext.
inline constexpr std::size_t sealNonceBytes = 12;
inline constexpr std::size_t sealTagBytes = 16;

/// `count` bytes from the system's generator; throws `std::runtime_error` when it has none.
[[nodiscard]] Bytes randomBytes(std::size_t count);

[[nodiscard]] Bytes sha256(std::span<const std::uint8_t> data);
[[nodiscard]] Bytes sha256(std::string_view text);

/// SHA-256 over bytes given a piece at a time, for a file too large to hold.
class Sha256 {
  public:
    /// Throws `std::runtime_error` when libcrypto cannot hash.
    Sha256();
    ~Sha256();
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;
    Sha256(Sha256&&) noexcept;
    Sha256& operator=(Sha256&&) noexcept;

    void add(std::string_view piece);
    /// The digest of everything added; the hash then starts over.
    [[nodiscard]] Bytes finish();

  private:
    struct Free {
        void operator()(evp_md_ctx_st* context) const noexcept;
    };
    std::unique_ptr<evp_md_ctx_st, Free> context_;
};

[[nodiscard]] Bytes hmacSha256(std::span<const std::uint8_t> key, std::string_view message);

/// RFC 5869 with SHA-256: `length` bytes of key from `secret`, `salt` and `info`.
[[nodiscard]] Bytes hkdfSha256(std::span<const std::uint8_t> secret,
                               std::span<const std::uint8_t> salt, std::string_view info,
                               std::size_t length);

/// `plain` encrypted and authenticated under the `keyBytes` key `key` and `nonce` with
/// ChaCha20-Poly1305 (RFC 8439), `aad` authenticated with it; the tag follows the ciphertext.
[[nodiscard]] std::string seal(std::span<const std::uint8_t> key,
                               std::span<const std::uint8_t> nonce, std::string_view aad,
                               std::string_view plain);

/// What `sealed` holds; nothing when it was not sealed under `key`, `nonce` and `aad` or was
/// changed since.
[[nodiscard]] std::optional<std::string> open(std::span<const std::uint8_t> key,
                                              std::span<const std::uint8_t> nonce,
                                              std::string_view aad, std::string_view sealed);

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
