// sdl_pad_ini — `[PadN]` sections that bind a pad's inputs as `SDL-<device>/<input>`, as PCSX2
// and DuckStation keep them, bound to the virtual pads for a launch. The device is an SDL player
// index or, in DuckStation, a persistent identifier; a launch lets SDL see only the virtual pads,
// so pad N plays `SDL-<N-1>`. A pad with SDL bindings keeps its inputs; a pad of a standard type
// bound only to the keyboard or to nothing gets the emulator's own SDL layout.
#pragma once

#include <span>
#include <string>
#include <string_view>

namespace opensu::library {

/// An input of the emulated pad and the SDL gamepad input the emulator's own layout gives it.
struct SdlPadDefault {
    std::string_view key;
    std::string_view input;
};

/// How one emulator lays out its pads.
struct SdlPadLayout {
    /// The log's tag.
    std::string_view emulator;
    /// The pads its settings bind, `[Pad1]` up.
    int pads{};
    /// The types the standard layout fits.
    std::span<const std::string_view> standardTypes;
    /// The type pad 1 has when its section names none; the others have none.
    std::string_view firstPadType;
    /// The standard layout.
    std::span<const SdlPadDefault> defaults;
};

/// `binding` with each `SDL-<device>/` made `device`; the rest kept.
[[nodiscard]] std::string sdlBindingOn(std::string_view binding, std::string_view device);

/// The `[PadN]` sections of `ini` bound to the virtual pads as `layout` lays them out.
[[nodiscard]] std::string sdlPadSettings(std::string_view ini, const SdlPadLayout& layout);

} // namespace opensu::library
