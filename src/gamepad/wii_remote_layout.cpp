#include "wii_remote_layout.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include <linux/input-event-codes.h>

namespace opensu::gamepad {
namespace {

constexpr std::uint16_t nintendo = 0x057e;
// RVL-CNT-01 and RVL-CNT-01-TR.
constexpr std::uint16_t wiiRemote = 0x0306;
constexpr std::uint16_t wiiRemotePlus = 0x0330;
constexpr std::string_view remoteName{"Nintendo Wii Remote"};
constexpr std::string_view nunchukName{"Nintendo Wii Remote Nunchuk"};
constexpr std::string_view classicName{"Nintendo Wii Remote Classic Controller"};
constexpr std::string_view cameraName{"Nintendo Wii Remote IR"};
// The camera's picture, in dots.
constexpr float cameraWidth = 1023.0f;
constexpr float cameraHeight = 767.0f;

using KeyMap = std::pair<std::uint16_t, std::uint16_t>;

// Sideways, the d-pad on the left: the remote's up points left.
constexpr std::array sideways{
    KeyMap{KEY_UP, BTN_DPAD_LEFT},   KeyMap{KEY_DOWN, BTN_DPAD_RIGHT},
    KeyMap{KEY_LEFT, BTN_DPAD_DOWN}, KeyMap{KEY_RIGHT, BTN_DPAD_UP},
    KeyMap{BTN_2, BTN_SOUTH},        KeyMap{BTN_1, BTN_EAST},
    KeyMap{BTN_B, BTN_WEST},         KeyMap{BTN_A, BTN_NORTH},
    KeyMap{KEY_NEXT, BTN_START},     KeyMap{KEY_PREVIOUS, BTN_SELECT},
    KeyMap{BTN_MODE, BTN_MODE},
};

constexpr std::array upright{
    KeyMap{KEY_UP, BTN_DPAD_UP},     KeyMap{KEY_DOWN, BTN_DPAD_DOWN},
    KeyMap{KEY_LEFT, BTN_DPAD_LEFT}, KeyMap{KEY_RIGHT, BTN_DPAD_RIGHT},
    KeyMap{BTN_A, BTN_SOUTH},        KeyMap{BTN_B, BTN_EAST},
    KeyMap{BTN_1, BTN_WEST},         KeyMap{BTN_2, BTN_NORTH},
    KeyMap{KEY_NEXT, BTN_START},     KeyMap{KEY_PREVIOUS, BTN_SELECT},
    KeyMap{BTN_MODE, BTN_MODE},
};

constexpr std::array nunchukKeys{
    KeyMap{BTN_C, BTN_TL},
    KeyMap{BTN_Z, BTN_TL2},
};

// hid-wiimote names the Classic's buttons for their labels: BTN_B is its B, at the bottom; L and
// R fully in are BTN_TL and BTN_TR, ZL and ZR are BTN_TL2 and BTN_TR2. The pad's BTN_X is the
// west button and BTN_Y the north, as the translator reads them.
constexpr std::array classicKeys{
    KeyMap{KEY_UP, BTN_DPAD_UP},     KeyMap{KEY_DOWN, BTN_DPAD_DOWN},
    KeyMap{KEY_LEFT, BTN_DPAD_LEFT}, KeyMap{KEY_RIGHT, BTN_DPAD_RIGHT},
    KeyMap{BTN_B, BTN_SOUTH},        KeyMap{BTN_A, BTN_EAST},
    KeyMap{BTN_Y, BTN_X},            KeyMap{BTN_X, BTN_Y},
    KeyMap{BTN_TL2, BTN_TL},         KeyMap{BTN_TR2, BTN_TR},
    KeyMap{BTN_TL, BTN_TL2},         KeyMap{BTN_TR, BTN_TR2},
    KeyMap{KEY_NEXT, BTN_START},     KeyMap{KEY_PREVIOUS, BTN_SELECT},
    KeyMap{BTN_MODE, BTN_MODE},
};

/// An extension's axis and the pad's it becomes; `flip` for one that grows upwards.
struct AxisMap {
    std::uint16_t from;
    std::uint16_t to;
    bool flip;
};

constexpr std::array nunchukAxes{
    AxisMap{ABS_HAT0X, ABS_X, false},
    AxisMap{ABS_HAT0Y, ABS_Y, true},
};

// The triggers' travel (ABS_HAT3) is left out: L and R are read fully in.
constexpr std::array classicAxes{
    AxisMap{ABS_HAT1X, ABS_X, false},
    AxisMap{ABS_HAT1Y, ABS_Y, true},
    AxisMap{ABS_HAT2X, ABS_RX, false},
    AxisMap{ABS_HAT2Y, ABS_RY, true},
};

template <std::size_t N>
std::optional<std::uint16_t> keyIn(const std::array<KeyMap, N>& keys, std::uint16_t code) {
    const auto found = std::ranges::find(keys, code, &KeyMap::first);
    if (found == keys.end()) {
        return std::nullopt;
    }
    return found->second;
}

} // namespace

std::optional<WiiRemotePart> wiiRemotePartOf(std::uint16_t vendor, std::uint16_t product,
                                             std::string_view name) {
    if (vendor != nintendo || (product != wiiRemote && product != wiiRemotePlus)) {
        return std::nullopt;
    }
    if (name == remoteName) {
        return WiiRemotePart::Remote;
    }
    if (name == nunchukName) {
        return WiiRemotePart::Nunchuk;
    }
    if (name == classicName) {
        return WiiRemotePart::Classic;
    }
    if (name == cameraName) {
        return WiiRemotePart::Camera;
    }
    return std::nullopt;
}

WiiRemoteLayout::WiiRemoteLayout(WiiRemotePart extension, const PadCapabilities& reports)
    : extension_{extension} {
    for (const auto& [code, range] : reports.axes) {
        ranges_[code] = range;
    }
}

namespace {

template <std::size_t N> void addKeys(PadCapabilities& pad, const std::array<KeyMap, N>& keys) {
    for (const KeyMap& key : keys) {
        pad.keys.insert(key.second);
    }
}

template <std::size_t N>
std::optional<PadEvent> axisIn(const std::array<AxisMap, N>& axes,
                               const std::map<std::uint16_t, AxisRange>& ranges,
                               const PadEvent& in) {
    const auto found = std::ranges::find(axes, in.code, &AxisMap::from);
    if (found == axes.end()) {
        return std::nullopt;
    }
    const auto range = ranges.find(in.code);
    const AxisRange span = range != ranges.end() ? range->second : AxisRange{-1, 1};
    // A gamepad's y grows downwards.
    const std::int32_t value = found->flip ? span.min + span.max - in.value : in.value;
    return PadEvent{.type = EV_ABS, .code = found->to, .value = value};
}

template <std::size_t N>
void addAxes(PadCapabilities& pad, const std::array<AxisMap, N>& axes,
             const std::map<std::uint16_t, AxisRange>& ranges) {
    for (const AxisMap& axis : axes) {
        const auto range = ranges.find(axis.from);
        pad.axes[axis.to] = range != ranges.end() ? range->second : AxisRange{-1, 1};
    }
}

} // namespace

PadCapabilities WiiRemoteLayout::capabilities() const {
    PadCapabilities pad;
    if (extension_ == WiiRemotePart::Classic) {
        addKeys(pad, classicKeys);
        addAxes(pad, classicAxes, ranges_);
        return pad;
    }
    if (extension_ == WiiRemotePart::Nunchuk) {
        addKeys(pad, upright);
        addKeys(pad, nunchukKeys);
        addAxes(pad, nunchukAxes, ranges_);
        return pad;
    }
    addKeys(pad, sideways);
    return pad;
}

std::optional<PadEvent> WiiRemoteLayout::map(WiiRemotePart part, const PadEvent& in) const {
    if (in.type == EV_SYN) {
        return in;
    }
    std::optional<std::uint16_t> key;
    if (part == WiiRemotePart::Remote && in.type == EV_KEY) {
        if (extension_ == WiiRemotePart::Classic) {
            // The remote hangs off the Classic Controller.
            return std::nullopt;
        }
        key = extension_ == WiiRemotePart::Nunchuk ? keyIn(upright, in.code)
                                                   : keyIn(sideways, in.code);
    } else if (part != extension_) {
        // The camera, the accelerometer, or an extension the layout is not for.
        return std::nullopt;
    } else if (in.type == EV_KEY) {
        key = part == WiiRemotePart::Classic ? keyIn(classicKeys, in.code)
                                             : keyIn(nunchukKeys, in.code);
    } else if (in.type == EV_ABS) {
        // The Nunchuk's accelerometer is not a stick.
        return part == WiiRemotePart::Classic ? axisIn(classicAxes, ranges_, in)
                                              : axisIn(nunchukAxes, ranges_, in);
    }
    if (!key) {
        return std::nullopt;
    }
    return PadEvent{.type = EV_KEY, .code = *key, .value = in.value};
}

std::optional<WiiAim> WiiPointer::feed(const PadEvent& in) {
    if (in.type == EV_ABS && in.code >= ABS_HAT0X && in.code <= ABS_HAT3Y) {
        dots_.at(in.code - ABS_HAT0X) = in.value;
        return std::nullopt;
    }
    if (in.type != EV_SYN || in.code != SYN_REPORT) {
        return std::nullopt;
    }
    float sumX = 0.0f;
    float sumY = 0.0f;
    int seen = 0;
    for (std::size_t dot = 0; dot < dots_.size() && seen < 2; dot += 2) {
        if (dots_.at(dot) == noDot || dots_.at(dot + 1) == noDot) {
            continue;
        }
        sumX += static_cast<float>(dots_.at(dot));
        sumY += static_cast<float>(dots_.at(dot + 1));
        ++seen;
    }
    WiiAim aim;
    if (seen > 0) {
        const auto count = static_cast<float>(seen);
        // The camera sees the bar mirrored across: pointing right moves the dots left.
        const float x = range_.across(1.0f - sumX / count / cameraWidth);
        const float y = range_.down(sumY / count / cameraHeight);
        if (x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f) {
            aim = WiiAim{.onScreen = true, .x = x, .y = y};
        }
    }
    if (aim == last_) {
        return std::nullopt;
    }
    last_ = aim;
    return aim;
}

bool WiiPointer::press(bool down) {
    if (down) {
        pressing_ = last_.onScreen;
        return pressing_;
    }
    return std::exchange(pressing_, false);
}

} // namespace opensu::gamepad
