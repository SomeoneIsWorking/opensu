#include "integrations.hpp"

#include <algorithm>

namespace opensu::settings {

std::string_view key(SteamMode mode) noexcept {
    return mode == SteamMode::Managed ? "managed" : "normal";
}

std::optional<SteamMode> steamModeOf(std::string_view spelling) noexcept {
    if (spelling == "normal") {
        return SteamMode::Normal;
    }
    if (spelling == "managed") {
        return SteamMode::Managed;
    }
    return std::nullopt;
}

std::string_view label(SteamMode mode) noexcept {
    return mode == SteamMode::Managed ? "Managed" : "Normal";
}

std::string_view label(SteamUse use) noexcept {
    switch (use) {
    case SteamUse::Normal:
        return "Normal";
    case SteamUse::Managed:
        return "Managed";
    case SteamUse::Off:
        break;
    }
    return "Off";
}

std::vector<SteamMode> Integrations::modesOffered(bool loginSession) {
    if (loginSession) {
        return {SteamMode::Managed};
    }
    return {SteamMode::Normal, SteamMode::Managed};
}

SteamMode Integrations::defaultMode(bool loginSession) noexcept {
    return loginSession ? SteamMode::Managed : SteamMode::Normal;
}

Integrations Integrations::inForce(const std::optional<IntegrationChoices>& choices,
                                   bool loginSession) noexcept {
    if (!choices) {
        return {};
    }
    Integrations in{.epic = choices->epic,
                    .gog = choices->gog,
                    .emulators = choices->emulators,
                    .plugins = choices->plugins};
    if (choices->steam) {
        SteamMode mode = choices->steamMode.value_or(defaultMode(loginSession));
        const std::vector<SteamMode> offered = modesOffered(loginSession);
        if (std::ranges::find(offered, mode) == offered.end()) {
            mode = defaultMode(loginSession);
        }
        in.steam = mode == SteamMode::Managed ? SteamUse::Managed : SteamUse::Normal;
    }
    return in;
}

bool Integrations::lists(library::Source source) const noexcept {
    switch (source) {
    case library::Source::Steam:
        return steam != SteamUse::Off;
    case library::Source::Epic:
        return epic;
    case library::Source::Gog:
        return gog;
    case library::Source::Rom:
        break;
    }
    return true;
}

} // namespace opensu::settings
