#include "switch_crypto.hpp"

#include <climits>
#include <memory>

#include <openssl/evp.h>

namespace opensu::library::roms {
namespace {

struct CipherFree {
    void operator()(EVP_CIPHER_CTX* context) const {
        EVP_CIPHER_CTX_free(context);
    }
};
using Cipher = std::unique_ptr<EVP_CIPHER_CTX, CipherFree>;

/// One decryption of `data` in place with `cipher`, no padding.
bool decryptWith(const EVP_CIPHER* cipher, const std::uint8_t* key, const std::uint8_t* iv,
                 std::span<std::uint8_t> data) {
    if (data.size() > static_cast<std::size_t>(INT_MAX)) {
        return false;
    }
    const Cipher context{EVP_CIPHER_CTX_new()};
    if (!context || EVP_DecryptInit_ex(context.get(), cipher, nullptr, key, iv) != 1 ||
        EVP_CIPHER_CTX_set_padding(context.get(), 0) != 1) {
        return false;
    }
    int written = 0;
    if (EVP_DecryptUpdate(context.get(), data.data(), &written, data.data(),
                          static_cast<int>(data.size())) != 1) {
        return false;
    }
    int tail = 0;
    return EVP_DecryptFinal_ex(context.get(), data.data() + written, &tail) == 1 &&
           static_cast<std::size_t>(written) + static_cast<std::size_t>(tail) == data.size();
}

} // namespace

bool decryptEcb(const Key128& key, std::span<std::uint8_t> data) {
    if (data.size() % aesBlock != 0) {
        return false;
    }
    return data.empty() || decryptWith(EVP_aes_128_ecb(), key.bytes().data(), nullptr, data);
}

bool decryptCtr(const Key128& key, const CounterBlock& counter, std::span<std::uint8_t> data) {
    return data.empty() || decryptWith(EVP_aes_128_ctr(), key.bytes().data(), counter.data(), data);
}

bool decryptXts(const Key256& key, std::uint64_t firstSector, std::size_t sectorSize,
                std::span<std::uint8_t> data) {
    if (sectorSize == 0 || data.size() % sectorSize != 0) {
        return false;
    }
    std::uint64_t sector = firstSector;
    for (std::size_t at = 0; at < data.size(); at += sectorSize) {
        std::array<std::uint8_t, aesBlock> tweak{};
        std::uint64_t number = sector;
        for (std::size_t i = tweak.size(); i > tweak.size() - sizeof(number); --i) {
            tweak[i - 1] = static_cast<std::uint8_t>(number & 0xffU);
            number >>= CHAR_BIT;
        }
        if (!decryptWith(EVP_aes_128_xts(), key.bytes().data(), tweak.data(),
                         data.subspan(at, sectorSize))) {
            return false;
        }
        ++sector;
    }
    return true;
}

} // namespace opensu::library::roms
