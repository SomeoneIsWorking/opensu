// switch_crypto — the AES-128 modes Switch content is encrypted in, over libcrypto: ECB for keys,
// CTR for NCA sections, XTS with Nintendo's big-endian sector tweak for NCA headers. Key bytes are
// wiped when they go.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "secrets/secret.hpp"

namespace opensu::library::roms {

/// Key material that is wiped when it goes out of scope.
template <std::size_t N> class KeyBytes {
  public:
    KeyBytes() = default;
    KeyBytes(const KeyBytes&) = default;
    KeyBytes& operator=(const KeyBytes&) = default;
    KeyBytes(KeyBytes&&) noexcept = default;
    KeyBytes& operator=(KeyBytes&&) noexcept = default;
    ~KeyBytes() {
        secrets::wipeBytes(bytes_.data(), bytes_.size());
    }

    [[nodiscard]] std::span<std::uint8_t, N> bytes() noexcept {
        return bytes_;
    }
    [[nodiscard]] std::span<const std::uint8_t, N> bytes() const noexcept {
        return bytes_;
    }

  private:
    std::array<std::uint8_t, N> bytes_{};
};

using Key128 = KeyBytes<16>;
/// An XTS key: the data key, then the tweak key.
using Key256 = KeyBytes<32>;

/// The 16-byte counter block of AES-CTR.
using CounterBlock = std::array<std::uint8_t, 16>;

/// The AES block size every mode here works in.
inline constexpr std::size_t aesBlock = 16;

/// Decrypts whole blocks in place with AES-128-ECB. False when `data` is not whole blocks or
/// libcrypto refuses.
[[nodiscard]] bool decryptEcb(const Key128& key, std::span<std::uint8_t> data);

/// Decrypts in place with AES-128-CTR starting at `counter`.
[[nodiscard]] bool decryptCtr(const Key128& key, const CounterBlock& counter,
                              std::span<std::uint8_t> data);

/// Decrypts whole sectors of `sectorSize` in place with AES-128-XTS, the first being sector
/// `firstSector`; each sector's tweak is its number as a big-endian 128-bit value, as Nintendo's
/// NCA headers use it.
[[nodiscard]] bool decryptXts(const Key256& key, std::uint64_t firstSector, std::size_t sectorSize,
                              std::span<std::uint8_t> data);

} // namespace opensu::library::roms
