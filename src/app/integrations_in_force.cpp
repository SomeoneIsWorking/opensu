#include "integrations_in_force.hpp"

#include "lucent/log.h"
#include "steam/client_modes.hpp"

namespace opensu::app {
namespace {

settings::SteamMode settingOf(steam::Client::Mode mode) noexcept {
    switch (mode) {
    case steam::Client::Mode::Managed:
        return settings::SteamMode::Managed;
    case steam::Client::Mode::Isolated:
        return settings::SteamMode::Isolated;
    case steam::Client::Mode::Normal:
        break;
    }
    return settings::SteamMode::Normal;
}

std::vector<settings::SteamMode> builtModes() {
    std::vector<settings::SteamMode> built;
    for (const steam::Client::Mode mode : steam::clientModes()) {
        built.push_back(settingOf(mode));
    }
    return built;
}

} // namespace

IntegrationsInForce::IntegrationsInForce(Preferences& preferences, bool loginSession)
    : preferences_{preferences}, loginSession_{loginSession}, built_{builtModes()},
      current_{settings::Integrations::inForce(preferences.values().integrations, loginSession,
                                               built_)} {
}

bool IntegrationsInForce::choose(const settings::IntegrationChoices& choices) {
    preferences_.values().integrations = choices;
    preferences_.save();
    const settings::Integrations next =
        settings::Integrations::inForce(preferences_.values().integrations, loginSession_, built_);
    if (next == current_) {
        return false;
    }
    current_ = next;
    lucent::info("integrations",
                 "in force: steam={} steamPads={} epic={} gog={} emulators={} plugins={}",
                 settings::label(current_.steam), settings::key(current_.steamPads), current_.epic,
                 current_.gog, current_.emulators, current_.plugins);
    return true;
}

library::CatalogStores IntegrationsInForce::catalogStores() const noexcept {
    return library::CatalogStores{.steam = current_.lists(library::Source::Steam),
                                  .epic = current_.lists(library::Source::Epic),
                                  .gog = current_.lists(library::Source::Gog),
                                  .standaloneEmulators = current_.emulators};
}

std::optional<steam::Client::Mode> IntegrationsInForce::steamMode() const noexcept {
    switch (current_.steam) {
    case settings::SteamUse::Normal:
        return steam::Client::Mode::Normal;
    case settings::SteamUse::Managed:
        return steam::Client::Mode::Managed;
    case settings::SteamUse::Isolated:
        return steam::Client::Mode::Isolated;
    case settings::SteamUse::Off:
        break;
    }
    return std::nullopt;
}

} // namespace opensu::app
