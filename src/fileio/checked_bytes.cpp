#include "checked_bytes.hpp"

namespace opensu::fileio {
namespace {

constexpr std::uint32_t replacement = 0xFFFDU;

void appendUtf8(std::string& out, std::uint32_t code) {
    if (code < 0x80U) {
        out += static_cast<char>(code);
    } else if (code < 0x800U) {
        out += static_cast<char>(0xC0U | (code >> 6U));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else if (code < 0x10000U) {
        out += static_cast<char>(0xE0U | (code >> 12U));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else {
        out += static_cast<char>(0xF0U | (code >> 18U));
        out += static_cast<char>(0x80U | ((code >> 12U) & 0x3FU));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    }
}

bool highSurrogate(std::uint32_t unit) noexcept {
    return unit >= 0xD800U && unit < 0xDC00U;
}

bool lowSurrogate(std::uint32_t unit) noexcept {
    return unit >= 0xDC00U && unit < 0xE000U;
}

} // namespace

std::string CheckedBytes::utf16(std::size_t at, std::size_t units) const {
    std::string out;
    for (std::size_t i = 0; i < units; ++i) {
        const std::size_t offset = at + (i * 2);
        const std::optional<std::uint16_t> unit = u16(offset);
        if (!unit || *unit == 0) {
            break;
        }
        if (highSurrogate(*unit)) {
            const std::optional<std::uint16_t> low = u16(offset + 2);
            if (i + 1 < units && low && lowSurrogate(*low)) {
                appendUtf8(out, 0x10000U + ((std::uint32_t{*unit} - 0xD800U) << 10U) +
                                    (std::uint32_t{*low} - 0xDC00U));
                ++i;
                continue;
            }
            appendUtf8(out, replacement);
        } else if (lowSurrogate(*unit)) {
            appendUtf8(out, replacement);
        } else {
            appendUtf8(out, *unit);
        }
    }
    return out;
}

std::string CheckedBytes::ansi(std::size_t at, std::size_t most) const {
    std::string out;
    for (std::size_t i = 0; i < most; ++i) {
        const std::optional<std::uint8_t> byte = u8(at + i);
        if (!byte || *byte == 0) {
            break;
        }
        appendUtf8(out, *byte);
    }
    return out;
}

} // namespace opensu::fileio
