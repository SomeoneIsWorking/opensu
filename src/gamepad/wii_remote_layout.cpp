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
    if (name == cameraName) {
        return WiiRemotePart::Camera;
    }
    return std::nullopt;
}

WiiRemoteLayout::WiiRemoteLayout(const PadCapabilities& nunchuk) {
    const auto range = [&nunchuk](std::uint16_t code) {
        const auto found = nunchuk.axes.find(code);
        return found != nunchuk.axes.end() ? found->second : AxisRange{-1, 1};
    };
    stick_ = Stick{.x = range(ABS_HAT0X), .y = range(ABS_HAT0Y)};
}

PadCapabilities WiiRemoteLayout::capabilities() const {
    PadCapabilities pad;
    for (const KeyMap& key : withNunchuk() ? upright : sideways) {
        pad.keys.insert(key.second);
    }
    if (stick_) {
        for (const KeyMap& key : nunchukKeys) {
            pad.keys.insert(key.second);
        }
        pad.axes[ABS_X] = stick_->x;
        pad.axes[ABS_Y] = stick_->y;
    }
    return pad;
}

std::optional<PadEvent> WiiRemoteLayout::map(WiiRemotePart part, const PadEvent& in) const {
    if (in.type == EV_SYN) {
        return in;
    }
    if (part == WiiRemotePart::Camera) {
        return std::nullopt;
    }
    if (part == WiiRemotePart::Remote) {
        if (in.type != EV_KEY) {
            return std::nullopt;
        }
        const std::optional<std::uint16_t> code =
            withNunchuk() ? keyIn(upright, in.code) : keyIn(sideways, in.code);
        if (!code) {
            return std::nullopt;
        }
        return PadEvent{.type = EV_KEY, .code = *code, .value = in.value};
    }
    if (!stick_) {
        return std::nullopt;
    }
    if (in.type == EV_KEY) {
        const std::optional<std::uint16_t> code = keyIn(nunchukKeys, in.code);
        if (!code) {
            return std::nullopt;
        }
        return PadEvent{.type = EV_KEY, .code = *code, .value = in.value};
    }
    if (in.type != EV_ABS) {
        return std::nullopt;
    }
    if (in.code == ABS_HAT0X) {
        return PadEvent{.type = EV_ABS, .code = ABS_X, .value = in.value};
    }
    if (in.code == ABS_HAT0Y) {
        // The Nunchuk reports up as positive; a gamepad's y grows downwards.
        return PadEvent{
            .type = EV_ABS, .code = ABS_Y, .value = stick_->y.min + stick_->y.max - in.value};
    }
    // The accelerometer is not a stick.
    return std::nullopt;
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
        aim = WiiAim{.onScreen = true,
                     .x = std::clamp(1.0f - sumX / count / cameraWidth, 0.0f, 1.0f),
                     .y = std::clamp(sumY / count / cameraHeight, 0.0f, 1.0f)};
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
