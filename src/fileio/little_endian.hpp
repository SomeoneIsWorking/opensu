// little_endian — unsigned integers as little-endian bytes, read from and written into byte
// strings: the Switch's file formats and remote play's packets.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace opensu::fileio {

/// The `size`-byte integer at `at`; `bytes` holds at least `at + size`.
[[nodiscard]] inline std::uint64_t littleEndian(std::span<const std::uint8_t> bytes, std::size_t at,
                                                std::size_t size) noexcept {
    std::uint64_t value = 0;
    for (std::size_t i = size; i > 0; --i) {
        value = (value << 8U) | bytes[at + i - 1];
    }
    return value;
}

[[nodiscard]] inline std::uint64_t littleEndian(std::string_view bytes, std::size_t at,
                                                std::size_t size) noexcept {
    return littleEndian(
        std::span{reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}, at, size);
}

/// The `T` at `at`.
template <class T>
    requires std::is_unsigned_v<T>
[[nodiscard]] T littleEndian(std::span<const std::uint8_t> bytes, std::size_t at) noexcept {
    return static_cast<T>(littleEndian(bytes, at, sizeof(T)));
}

/// Writes the low `size` bytes of `value` at `at`; `bytes` holds at least `at + size`.
inline void putLittleEndian(std::string& bytes, std::size_t at, std::uint64_t value,
                            std::size_t size) noexcept {
    for (std::size_t i = 0; i < size; ++i) {
        bytes[at + i] = static_cast<char>((value >> (8 * i)) & 0xFFU);
    }
}

/// Appends the low `size` bytes of `value`.
inline void appendLittleEndian(std::string& bytes, std::uint64_t value, std::size_t size) {
    const std::size_t at = bytes.size();
    bytes.resize(at + size);
    putLittleEndian(bytes, at, value, size);
}

} // namespace opensu::fileio
