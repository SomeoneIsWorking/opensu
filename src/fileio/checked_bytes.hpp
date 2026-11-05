// checked_bytes — reading a file nobody vouches for: every read says whether it fell inside the
// bytes, offsets and sizes are compared without overflowing, and text is decoded to UTF-8. The
// shortcut, PE and icon parsers read through it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "little_endian.hpp"

namespace opensu::fileio {

class CheckedBytes {
  public:
    explicit CheckedBytes(std::span<const std::uint8_t> bytes) noexcept : bytes_{bytes} {
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return bytes_.size();
    }
    /// Whether `size` bytes lie at `at`.
    [[nodiscard]] bool holds(std::size_t at, std::size_t size) const noexcept {
        return at <= bytes_.size() && size <= bytes_.size() - at;
    }
    [[nodiscard]] std::optional<std::uint8_t> u8(std::size_t at) const noexcept {
        return holds(at, 1) ? std::optional{bytes_[at]} : std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint16_t> u16(std::size_t at) const noexcept {
        return holds(at, 2) ? std::optional{littleEndian<std::uint16_t>(bytes_, at)} : std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint32_t> u32(std::size_t at) const noexcept {
        return holds(at, 4) ? std::optional{littleEndian<std::uint32_t>(bytes_, at)} : std::nullopt;
    }
    /// The `size` bytes at `at`; nothing when they are not all there.
    [[nodiscard]] std::optional<std::span<const std::uint8_t>>
    slice(std::size_t at, std::size_t size) const noexcept {
        return holds(at, size) ? std::optional{bytes_.subspan(at, size)} : std::nullopt;
    }

    /// The UTF-16LE text at `at`, to its terminator, `units` code units or the end of the bytes.
    /// A lone surrogate becomes U+FFFD.
    [[nodiscard]] std::string utf16(std::size_t at, std::size_t units) const;
    /// The single-byte text at `at`, to its terminator, `most` bytes or the end of the bytes. Each
    /// byte is taken as Latin-1: the code page it was written in is not recorded.
    [[nodiscard]] std::string ansi(std::size_t at, std::size_t most) const;

  private:
    std::span<const std::uint8_t> bytes_;
};

} // namespace opensu::fileio
