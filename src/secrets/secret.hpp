// secret — clearing memory that held a password or key, and keys written as hex.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::secrets {

/// Overwrites `size` bytes at `data` in a way the compiler keeps.
void wipeBytes(void* data, std::size_t size) noexcept;

/// Overwrites everything `text` has allocated, so a password typed into it (and anything a
/// backspace left past its end) does not outlive the buffer, then empties it.
inline void wipe(std::string& text) noexcept {
    // The string's buffer holds capacity() chars past data(), whatever its size.
    wipeBytes(text.data(), text.capacity());
    text.clear();
}

/// The bytes `hex` spells, two digits each, either case; nothing when it is empty, odd or holds
/// anything else, with what was read wiped.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> hexBytes(std::string_view hex);

/// Whether `text` is non-empty lower-case hex, as `hexText` writes it.
[[nodiscard]] bool isLowerHex(std::string_view text) noexcept;

/// `bytes` as lower-case hex, two digits each.
[[nodiscard]] std::string hexText(std::span<const std::uint8_t> bytes);

} // namespace opensu::secrets
