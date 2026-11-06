#include "arcade_cabinet.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <string_view>
#include <utility>
#include <vector>

#include <libretro.h>

#include "joypad.hpp"

namespace opensu::retro {
namespace {

using touch::ArcadeButton;

/// What a machine's buttons are read in when nothing names them: the RetroPad's face first.
constexpr std::array<unsigned, 8> buttonOrder{RETRO_DEVICE_ID_JOYPAD_B,  RETRO_DEVICE_ID_JOYPAD_A,
                                              RETRO_DEVICE_ID_JOYPAD_Y,  RETRO_DEVICE_ID_JOYPAD_X,
                                              RETRO_DEVICE_ID_JOYPAD_L,  RETRO_DEVICE_ID_JOYPAD_R,
                                              RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2};
/// With no word on a machine at all, enough to play most of them.
constexpr unsigned unknownButtons = 4;
/// A core's own conveniences and the operator's switches, none of them on the player's panel.
constexpr std::array<std::string_view, 7> notOnThePanel{"buttons ", "turbo", "service", "diag",
                                                        "reset",    "test",  "tilt"};
constexpr std::string_view buttonWord = "Button ";
constexpr std::size_t fighterButtons = 6;
/// The strength a core's name for a punch or kick starts with, weakest first.
constexpr std::string_view strengthOrder = "LMH";
constexpr std::array<std::pair<std::string_view, std::string_view>, 11> strengths{
    {{"weak", "L"},
     {"light", "L"},
     {"low", "L"},
     {"medium", "M"},
     {"mid", "M"},
     {"middle", "M"},
     {"strong", "H"},
     {"heavy", "H"},
     {"hard", "H"},
     {"high", "H"},
     {"fierce", "H"}}};

std::string lowerCase(std::string_view text) {
    std::string lower{text};
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower;
}

bool isPanelButton(const InputDescriptor& descriptor) {
    if (std::ranges::find(buttonOrder, descriptor.id) == buttonOrder.end()) {
        return false;
    }
    const std::string lower = lowerCase(descriptor.description);
    return std::ranges::none_of(notOnThePanel, [&lower](std::string_view word) {
        return lower.find(word) != std::string::npos;
    });
}

/// "Button 3": a place on a table, which says nothing of this game.
bool isNumbered(std::string_view description) {
    if (!description.starts_with(buttonWord) || description.size() == buttonWord.size()) {
        return false;
    }
    description.remove_prefix(buttonWord.size());
    return std::ranges::all_of(description, [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

/// "Button A" reads A on a button.
std::string labelOf(std::string_view description) {
    if (description.starts_with(buttonWord)) {
        description.remove_prefix(buttonWord.size());
    }
    return std::string{description};
}

/// `count` unnamed buttons.
/// A button named a punch or a kick, "Weak Punch" or plain "Kick", drawn so; a named move is not.
void readLimb(ArcadeButton& button, std::string_view description) {
    const std::string lower = lowerCase(description);
    const std::size_t space = lower.rfind(' ');
    const std::string_view word =
        std::string_view{lower}.substr(space == std::string::npos ? 0 : space + 1);
    const touch::Limb limb = word == "punch"  ? touch::Limb::Punch
                             : word == "kick" ? touch::Limb::Kick
                                              : touch::Limb::None;
    if (limb == touch::Limb::None) {
        return;
    }
    if (space == std::string::npos) {
        button.limb = limb;
        button.label.clear();
        return;
    }
    const std::string_view first = std::string_view{lower}.substr(0, space);
    const auto strength = std::ranges::find(strengths, first, [](const auto& entry) {
        return entry.first;
    });
    if (strength != strengths.end()) {
        button.limb = limb;
        button.label = strength->second;
    }
}

/// Punches above kicks, each weakest first, when every button is one of them.
void cabinetOrder(std::vector<ArcadeButton>& buttons) {
    const bool fighter = std::ranges::all_of(buttons, [](const ArcadeButton& button) {
        return button.limb != touch::Limb::None && !button.label.empty();
    });
    if (fighter) {
        std::ranges::stable_sort(buttons, {}, [](const ArcadeButton& button) {
            return std::pair{button.limb, strengthOrder.find(button.label)};
        });
    }
}

/// Six numbered buttons, the first three punches and the rest kicks, in strength order.
void numberedAsFighter(std::vector<ArcadeButton>& buttons) {
    for (std::size_t at = 0; at < buttons.size(); ++at) {
        const std::size_t half = fighterButtons / 2;
        buttons[at].limb = at < half ? touch::Limb::Punch : touch::Limb::Kick;
        buttons[at].label = strengthOrder.substr(at % half, 1);
    }
}

std::vector<ArcadeButton> numberedButtons(unsigned count) {
    std::vector<ArcadeButton> buttons;
    for (unsigned at = 0; at < std::min<std::size_t>(count, buttonOrder.size()); ++at) {
        if (const std::optional<gamepad::Button> button = Joypad::buttonOf(buttonOrder.at(at))) {
            buttons.push_back(ArcadeButton{*button, std::to_string(at + 1)});
        }
    }
    return buttons;
}

} // namespace

std::optional<CabinetListing> cabinetListingOf(std::string_view text) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    CabinetListing listing;
    const std::string_view buttons = text.substr(0, colon);
    std::string_view ways = text.substr(colon + 1);
    if (ways.ends_with('f')) {
        listing.fighter = true;
        ways.remove_suffix(1);
    }
    if (ways.starts_with('v')) {
        listing.vertical = true;
        ways.remove_prefix(1);
    }
    const auto buttonsEnd =
        std::from_chars(buttons.data(), buttons.data() + buttons.size(), listing.buttons);
    const auto waysEnd = std::from_chars(ways.data(), ways.data() + ways.size(), listing.ways);
    if (buttonsEnd.ec != std::errc{} || waysEnd.ec != std::errc{} ||
        buttonsEnd.ptr != buttons.data() + buttons.size() ||
        waysEnd.ptr != ways.data() + ways.size()) {
        return std::nullopt;
    }
    return listing;
}

touch::ArcadePanel panelOf(std::span<const InputDescriptor> descriptors,
                           const CabinetListing& listing) {
    touch::ArcadePanel panel;
    bool horizontal = false;
    bool vertical = false;
    bool numbered = true;
    for (const InputDescriptor& descriptor : descriptors) {
        horizontal = horizontal || descriptor.id == RETRO_DEVICE_ID_JOYPAD_LEFT ||
                     descriptor.id == RETRO_DEVICE_ID_JOYPAD_RIGHT;
        vertical = vertical || descriptor.id == RETRO_DEVICE_ID_JOYPAD_UP ||
                   descriptor.id == RETRO_DEVICE_ID_JOYPAD_DOWN;
        if (!isPanelButton(descriptor)) {
            continue;
        }
        if (const std::optional<gamepad::Button> button = Joypad::buttonOf(descriptor.id)) {
            numbered = numbered && isNumbered(descriptor.description);
            ArcadeButton named{*button, labelOf(descriptor.description)};
            readLimb(named, descriptor.description);
            panel.buttons.push_back(std::move(named));
        }
    }
    if (descriptors.empty()) {
        panel.buttons = numberedButtons(listing.buttons != 0 ? listing.buttons : unknownButtons);
    } else if (numbered && listing.buttons != 0 && listing.buttons < panel.buttons.size()) {
        panel.buttons.resize(listing.buttons);
    }
    if (numbered && listing.fighter && panel.buttons.size() == fighterButtons) {
        numberedAsFighter(panel.buttons);
    }
    cabinetOrder(panel.buttons);
    if (listing.ways != 0) {
        panel.ways = listing.ways;
        panel.vertical = listing.vertical;
    } else if (!descriptors.empty()) {
        panel.ways = horizontal && vertical ? 8 : (horizontal || vertical ? 2 : 0);
        panel.vertical = vertical && !horizontal;
    }
    return panel;
}

} // namespace opensu::retro
