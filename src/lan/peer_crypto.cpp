#include "peer_crypto.hpp"

#include <algorithm>
#include <climits>
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

struct CipherFree {
    void operator()(EVP_CIPHER_CTX* context) const noexcept {
        EVP_CIPHER_CTX_free(context);
    }
};
using Cipher = std::unique_ptr<EVP_CIPHER_CTX, CipherFree>;

/// A ChaCha20-Poly1305 context keyed for `encrypt` or decrypt, `aad` already taken; null when
/// libcrypto refuses.
Cipher aeadFor(bool encrypt, std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
               std::string_view aad) {
    if (key.size() != keyBytes || nonce.size() != sealNonceBytes || aad.size() > INT_MAX) {
        return nullptr;
    }
    Cipher context{EVP_CIPHER_CTX_new()};
    const int mode = encrypt ? 1 : 0;
    int written = 0;
    if (context == nullptr ||
        EVP_CipherInit_ex(context.get(), EVP_chacha20_poly1305(), nullptr, nullptr, nullptr,
                          mode) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_SET_IVLEN,
                            static_cast<int>(sealNonceBytes), nullptr) != 1 ||
        EVP_CipherInit_ex(context.get(), nullptr, nullptr, key.data(), nonce.data(), mode) != 1 ||
        EVP_CipherUpdate(context.get(), nullptr, &written, bytesOf(aad),
                         static_cast<int>(aad.size())) != 1) {
        return nullptr;
    }
    return context;
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

void Sha256::Free::operator()(evp_md_ctx_st* context) const noexcept {
    EVP_MD_CTX_free(context);
}

Sha256::Sha256() : context_{EVP_MD_CTX_new()} {
    if (context_ == nullptr || EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error{"libcrypto cannot hash with SHA-256"};
    }
}

Sha256::~Sha256() = default;
Sha256::Sha256(Sha256&&) noexcept = default;
Sha256& Sha256::operator=(Sha256&&) noexcept = default;

void Sha256::add(std::string_view piece) {
    if (EVP_DigestUpdate(context_.get(), piece.data(), piece.size()) != 1) {
        throw std::runtime_error{"libcrypto cannot hash with SHA-256"};
    }
}

Bytes Sha256::finish() {
    Bytes out(sha256Bytes);
    unsigned int written = 0;
    if (EVP_DigestFinal_ex(context_.get(), out.data(), &written) != 1 ||
        EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1) {
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

std::string seal(std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
                 std::string_view aad, std::string_view plain) {
    const Cipher context = aeadFor(true, key, nonce, aad);
    if (context == nullptr || plain.size() > INT_MAX - sealTagBytes) {
        throw std::runtime_error{"libcrypto cannot seal with ChaCha20-Poly1305"};
    }
    std::string sealed(plain.size() + sealTagBytes, '\0');
    auto* out = reinterpret_cast<unsigned char*>(sealed.data());
    int written = 0;
    int finished = 0;
    if (EVP_CipherUpdate(context.get(), out, &written, bytesOf(plain),
                         static_cast<int>(plain.size())) != 1 ||
        EVP_CipherFinal_ex(context.get(), out + written, &finished) != 1 ||
        static_cast<std::size_t>(written) + static_cast<std::size_t>(finished) != plain.size() ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_GET_TAG, static_cast<int>(sealTagBytes),
                            out + plain.size()) != 1) {
        throw std::runtime_error{"libcrypto cannot seal with ChaCha20-Poly1305"};
    }
    return sealed;
}

std::optional<std::string> open(std::span<const std::uint8_t> key,
                                std::span<const std::uint8_t> nonce, std::string_view aad,
                                std::string_view sealed) {
    if (sealed.size() < sealTagBytes || sealed.size() > INT_MAX) {
        return std::nullopt;
    }
    const Cipher context = aeadFor(false, key, nonce, aad);
    if (context == nullptr) {
        return std::nullopt;
    }
    const std::size_t length = sealed.size() - sealTagBytes;
    std::string tag{sealed.substr(length)};
    std::string plain(length, '\0');
    auto* out = reinterpret_cast<unsigned char*>(plain.data());
    int written = 0;
    int finished = 0;
    if (EVP_CipherUpdate(context.get(), out, &written, bytesOf(sealed), static_cast<int>(length)) !=
            1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_SET_TAG, static_cast<int>(sealTagBytes),
                            tag.data()) != 1 ||
        EVP_CipherFinal_ex(context.get(), out + written, &finished) != 1) {
        secrets::wipe(plain);
        return std::nullopt;
    }
    return plain;
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
