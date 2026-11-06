// arcade_cabinet — an arcade machine's control panel from data: what the core reports for the
// running game (RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS) first, then what the romset listing says
// (`--arcade`). FinalBurn Neo and FB Alpha 2012 name each game's own buttons; MAME 2010 sends one
// table of seven numbered buttons for every game, so the listing counts its buttons.
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "touch/arcade_layout.hpp"

namespace opensu::retro {

/// One of port 0's RETRO_DEVICE_JOYPAD controls as the core names it.
struct InputDescriptor {
    /// A RETRO_DEVICE_ID_JOYPAD_*.
    unsigned id{0};
    std::string description;

    bool operator==(const InputDescriptor&) const = default;
};

/// What a set's listing says of its controls; 0 where it says nothing.
struct CabinetListing {
    unsigned buttons{0};
    unsigned ways{0};
    /// A 2-way stick moves up and down.
    bool vertical{false};
    /// A Capcom fighter's board, its buttons numbered punches then kicks.
    bool fighter{false};

    bool operator==(const CabinetListing&) const = default;
};

/// The value of `--arcade`, `<buttons>:<ways>` with `v` before a vertical stick's ways: `6:8`,
/// `2:v2`, `0:0`; nothing when malformed.
[[nodiscard]] std::optional<CabinetListing> cabinetListingOf(std::string_view text);

/// The panel of a machine whose core reported `descriptors` and whose set lists `listing`: a stick
/// and the buttons they name, Coin and Start being the panel's own.
[[nodiscard]] touch::ArcadePanel panelOf(std::span<const InputDescriptor> descriptors,
                                         const CabinetListing& listing);

} // namespace opensu::retro
