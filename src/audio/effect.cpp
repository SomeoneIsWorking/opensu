#include "effect.hpp"

#include <algorithm>

namespace iideck::audio {

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
    }
    return {};
}

std::optional<Effect> effectOfFile(std::string_view file) noexcept {
    const auto found = std::ranges::find_if(allEffects, [file](Effect effect) {
        return assetFile(effect) == file;
    });
    return found == allEffects.end() ? std::nullopt : std::optional{*found};
}

std::chrono::milliseconds debounceOf(Effect effect) noexcept {
    switch (effect) {
    case Effect::EnterConsolesApps:
    case Effect::ExitConsolesApps:
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

} // namespace iideck::audio
