// sdl_guid — the GUID SDL gives a Linux evdev pad, as `SDL_GUIDToString` writes it: bus, CRC-16 of
// the device name, vendor, product and version, each little-endian, the rest zero.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace opensu::gamepad {

/// What SDL builds a pad's GUID from: its evdev `input_id` and name.
struct PadIdentity {
    std::uint16_t bus{0};
    std::uint16_t vendor{0};
    std::uint16_t product{0};
    std::uint16_t version{0};
    std::string_view name;
};

/// 32 lowercase hex digits, `0300cece5e0400008e02000010010000` for openSU's virtual pad.
[[nodiscard]] std::string sdlGuid(const PadIdentity& pad);

} // namespace opensu::gamepad
