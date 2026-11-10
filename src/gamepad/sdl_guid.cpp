#include "sdl_guid.hpp"

#include <array>
#include <cstddef>

namespace opensu::gamepad {
namespace {

/// SDL_crc16: CRC-16/ARC, reflected polynomial 0xA001, starting at 0.
std::uint16_t crc16(std::string_view text) {
    std::uint16_t crc = 0;
    for (const char c : text) {
        crc ^= static_cast<std::uint8_t>(c);
        for (int bit = 0; bit < 8; ++bit) {
            const bool low = (crc & 1U) != 0;
            crc = static_cast<std::uint16_t>(crc >> 1U);
            if (low) {
                crc ^= 0xA001U;
            }
        }
    }
    return crc;
}

} // namespace

std::string sdlGuid(const PadIdentity& pad) {
    std::array<std::uint8_t, 16> bytes{};
    const std::array<std::uint16_t, 8> words{pad.bus, crc16(pad.name), pad.vendor, 0, pad.product,
                                             0,       pad.version,     0};
    for (std::size_t i = 0; i < words.size(); ++i) {
        bytes[2 * i] = static_cast<std::uint8_t>(words[i] & 0xFFU);
        bytes[2 * i + 1] = static_cast<std::uint8_t>(words[i] >> 8U);
    }
    constexpr std::string_view digits{"0123456789abcdef"};
    std::string out;
    out.reserve(2 * bytes.size());
    for (const std::uint8_t byte : bytes) {
        out += digits[byte >> 4U];
        out += digits[byte & 0xFU];
    }
    return out;
}

} // namespace opensu::gamepad
