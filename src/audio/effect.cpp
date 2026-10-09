#include "effect.hpp"

#include <algorithm>

namespace opensu::audio {

std::string_view assetFile(Effect effect) noexcept {
    switch (effect) {
    case Effect::Open:
        return "Open.wav";
    case Effect::Close:
        return "Close.wav";
    case Effect::OpenContextMenu:
        return "Open ContextMenu.wav";
    case Effect::Navigation:
        return "Navigation.wav";
    case Effect::OpenAppRom:
        return "Open AppRom.wav";
    case Effect::EnterConsolesApps:
        return "Enter ConsolesApps.wav";
    case Effect::ExitConsolesApps:
        return "Exit ConsolesApps.wav";
    case Effect::DominoOne:
        return "domino_icons_1.ogg";
    case Effect::DominoTwo:
        return "domino_icons_2.ogg";
    case Effect::DominoThreeToFive:
        return "domino_icons_0_5.ogg";
    case Effect::DominoSixToEleven:
        return "domino_icons_6_11.ogg";
    case Effect::DominoTwelvePlus:
        return "domino_icons_12_plus.ogg";
    }
    return {};
}

std::optional<Effect> effectOfFile(std::string_view file) noexcept {
    const auto found = std::ranges::find_if(allEffects, [file](Effect effect) {
        return assetFile(effect) == file;
    });
    return found == allEffects.end() ? std::nullopt : std::optional{*found};
}

std::optional<Effect> dominoFor(std::size_t tiles) noexcept {
    if (tiles == 0) {
        return std::nullopt;
    }
    if (tiles == 1) {
        return Effect::DominoOne;
    }
    if (tiles == 2) {
        return Effect::DominoTwo;
    }
    if (tiles <= 5) {
        return Effect::DominoThreeToFive;
    }
    return tiles <= 11 ? Effect::DominoSixToEleven : Effect::DominoTwelvePlus;
}

std::chrono::milliseconds debounceOf(Effect effect) noexcept {
    switch (effect) {
    case Effect::EnterConsolesApps:
    case Effect::ExitConsolesApps:
    case Effect::DominoOne:
    case Effect::DominoTwo:
    case Effect::DominoThreeToFive:
    case Effect::DominoSixToEleven:
    case Effect::DominoTwelvePlus:
        return std::chrono::milliseconds{91};
    case Effect::Open:
    case Effect::Close:
    case Effect::OpenContextMenu:
    case Effect::Navigation:
    case Effect::OpenAppRom:
        break;
    }
    return std::chrono::milliseconds{0};
}

} // namespace opensu::audio
