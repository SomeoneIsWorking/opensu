#include "peer_crypto.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include "secrets/secret.hpp"

namespace opensu::lan::crypto {
namespace {

constexpr std::size_t sha256Bytes = 32;

struct ContextFree {
    void operator()(EVP_PKEY_CTX* context) const noexcept {
        EVP_PKEY_CTX_free(context);
    }
};
using Context = std::unique_ptr<EVP_PKEY_CTX, ContextFree>;

const unsigned char* bytesOf(std::string_view text) noexcept {
    return reinterpret_cast<const unsigned char*>(text.data());
}

} // namespace

Bytes randomBytes(std::size_t count) {
    Bytes out(count);
    if (RAND_bytes(out.data(), static_cast<int>(out.size())) != 1) {
        throw std::runtime_error{"the system has no random bytes to give"};
    }
    return out;
}

Bytes sha256(std::span<const std::uint8_t> data) {
    return sha256(asText(data));
}

Bytes sha256(std::string_view text) {
    Bytes out(sha256Bytes);
    unsigned int written = 0;
    if (EVP_Digest(text.data(), text.size(), out.data(), &written, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error{"libcrypto cannot hash with SHA-256"};
    }
    return out;
}

Bytes hmacSha256(std::span<const std::uint8_t> key, std::string_view message) {
    Bytes out(sha256Bytes);
    std::size_t written = 0;
    if (EVP_Q_mac(nullptr, "HMAC", nullptr, "SHA256", nullptr, key.data(), key.size(),
                  bytesOf(message), message.size(), out.data(), out.size(), &written) == nullptr ||
        written != out.size()) {
        throw std::runtime_error{"libcrypto cannot make an HMAC-SHA-256"};
    }
    return out;
}

Bytes hkdfSha256(std::span<const std::uint8_t> secret, std::span<const std::uint8_t> salt,
                 std::string_view info, std::size_t length) {
    const Context context{EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr)};
    Bytes out(length);
    std::size_t written = out.size();
    if (context == nullptr || EVP_PKEY_derive_init(context.get()) != 1 ||
        EVP_PKEY_CTX_set_hkdf_md(context.get(), EVP_sha256()) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_salt(context.get(), salt.data(), static_cast<int>(salt.size())) !=
            1 ||
        EVP_PKEY_CTX_set1_hkdf_key(context.get(), secret.data(), static_cast<int>(secret.size())) !=
            1 ||
        EVP_PKEY_CTX_add1_hkdf_info(context.get(), bytesOf(info), static_cast<int>(info.size())) !=
            1 ||
        EVP_PKEY_derive(context.get(), out.data(), &written) != 1 || written != length) {
        secrets::wipeBytes(out.data(), out.size());
        throw std::runtime_error{"libcrypto cannot derive a key with HKDF"};
    }
    return out;
}

bool sameBytes(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) noexcept {
    return a.size() == b.size() && CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

std::string_view asText(std::span<const std::uint8_t> bytes) noexcept {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

void KeyPair::Free::operator()(evp_pkey_st* key) const noexcept {
    EVP_PKEY_free(key);
}

KeyPair::KeyPair() : key_{EVP_PKEY_Q_keygen(nullptr, nullptr, "X25519")} {
    std::size_t length = keyBytes;
    public_.resize(keyBytes);
    if (key_ == nullptr || EVP_PKEY_get_raw_public_key(key_.get(), public_.data(), &length) != 1 ||
        length != keyBytes) {
        throw std::runtime_error{"libcrypto cannot make an X25519 key"};
    }
}

KeyPair::~KeyPair() = default;
KeyPair::KeyPair(KeyPair&&) noexcept = default;
KeyPair& KeyPair::operator=(KeyPair&&) noexcept = default;

std::optional<Bytes> KeyPair::agree(std::span<const std::uint8_t> peerPublic) const {
    if (peerPublic.size() != keyBytes) {
        return std::nullopt;
    }
    const std::unique_ptr<EVP_PKEY, Free> peer{EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr, peerPublic.data(), peerPublic.size())};
    if (peer == nullptr) {
        return std::nullopt;
    }
    const Context context{EVP_PKEY_CTX_new(key_.get(), nullptr)};
    Bytes shared(keyBytes);
    std::size_t length = shared.size();
    if (context == nullptr || EVP_PKEY_derive_init(context.get()) != 1 ||
        EVP_PKEY_derive_set_peer(context.get(), peer.get()) != 1 ||
        EVP_PKEY_derive(context.get(), shared.data(), &length) != 1 || length != keyBytes) {
        return std::nullopt;
    }
    // A small-order key gives the all-zero secret, which anyone could compute.
    if (std::ranges::all_of(shared, [](std::uint8_t byte) {
            return byte == 0;
        })) {
        return std::nullopt;
    }
    return shared;
}

} // namespace opensu::lan::crypto
