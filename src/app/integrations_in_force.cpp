#include "integrations_in_force.hpp"

#include "lucent/log.h"

namespace opensu::app {

bool IntegrationsInForce::choose(const settings::IntegrationChoices& choices) {
    preferences_.values().integrations = choices;
    preferences_.save();
    const settings::Integrations next =
        settings::Integrations::inForce(preferences_.values().integrations, loginSession_);
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
