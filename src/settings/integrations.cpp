#include "integrations.hpp"

#include <algorithm>

namespace opensu::settings {

std::string_view key(SteamMode mode) noexcept {
    switch (mode) {
    case SteamMode::Managed:
        return "managed";
    case SteamMode::Isolated:
        return "isolated";
    case SteamMode::Normal:
        break;
    }
    return "normal";
}

std::optional<SteamMode> steamModeOf(std::string_view spelling) noexcept {
    for (const SteamMode mode : {SteamMode::Normal, SteamMode::Managed, SteamMode::Isolated}) {
        if (spelling == key(mode)) {
            return mode;
        }
    }
    return std::nullopt;
}

std::string_view label(SteamMode mode) noexcept {
    switch (mode) {
    case SteamMode::Managed:
        return "Managed";
    case SteamMode::Isolated:
        return "Isolated";
    case SteamMode::Normal:
        break;
    }
    return "Normal";
}

std::string_view key(SteamPads pads) noexcept {
    return pads == SteamPads::All ? "all" : "virtual";
}

std::optional<SteamPads> steamPadsOf(std::string_view spelling) noexcept {
    for (const SteamPads pads : {SteamPads::VirtualOnly, SteamPads::All}) {
        if (spelling == key(pads)) {
            return pads;
        }
    }
    return std::nullopt;
}

std::string_view label(SteamPads pads) noexcept {
    return pads == SteamPads::All ? "Every controller" : "openSU's virtual pads";
}

std::string_view note(SteamPads pads) noexcept {
    return pads == SteamPads::All ? "openSU grabs them while it is in front"
                                  : "Your controllers stay hidden from Steam";
}

std::string_view label(SteamUse use) noexcept {
    switch (use) {
    case SteamUse::Normal:
        return "Normal";
    case SteamUse::Managed:
        return "Managed";
    case SteamUse::Isolated:
        return "Isolated";
    case SteamUse::Off:
        break;
    }
    return "Off";
}

std::vector<SteamMode> Integrations::modesOffered(bool loginSession,
                                                  std::span<const SteamMode> built) {
    std::vector<SteamMode> offered;
    for (const SteamMode mode : built) {
        if (!(loginSession && mode == SteamMode::Normal)) {
            offered.push_back(mode);
        }
    }
    return offered;
}

SteamMode Integrations::defaultMode(bool loginSession) noexcept {
    return loginSession ? SteamMode::Managed : SteamMode::Normal;
}

std::vector<SteamPads> Integrations::padsOffered() {
    return {SteamPads::VirtualOnly, SteamPads::All};
}

Integrations Integrations::inForce(const std::optional<IntegrationChoices>& choices,
                                   bool loginSession, std::span<const SteamMode> built) {
    if (!choices) {
        return {};
    }
    Integrations in{.epic = choices->epic,
                    .gog = choices->gog,
                    .emulators = choices->emulators,
                    .plugins = choices->plugins};
    if (choices->steam) {
        SteamMode mode = choices->steamMode.value_or(defaultMode(loginSession));
        const std::vector<SteamMode> offered = modesOffered(loginSession, built);
        if (std::ranges::find(offered, mode) == offered.end()) {
            mode = defaultMode(loginSession);
        }
        switch (mode) {
        case SteamMode::Normal:
            in.steam = SteamUse::Normal;
            break;
        case SteamMode::Managed:
            in.steam = SteamUse::Managed;
            break;
        case SteamMode::Isolated:
            in.steam = SteamUse::Isolated;
            in.steamPads = choices->steamPads.value_or(padsOffered().front());
            break;
        }
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
