// integrations_in_force — the one owner of which integrations and which Steam mode are in force:
// the player's choices from the preferences under `settings::Integrations::inForce`, what each
// consumer is given of them (the catalog's stores, the Steam client's mode), and a new choice
// kept. Everything that starts, reads or shows an integration asks this, nothing else.
#pragma once

#include <optional>

#include "library/catalog.hpp"
#include "preferences.hpp"
#include "settings/integrations.hpp"
#include "steam/client.hpp"

namespace opensu::app {

class IntegrationsInForce {
  public:
    /// `loginSession` is openSU running as the login session (`--session`).
    IntegrationsInForce(Preferences& preferences, bool loginSession)
        : preferences_{preferences}, loginSession_{loginSession},
          current_{
              settings::Integrations::inForce(preferences.values().integrations, loginSession)} {
    }

    [[nodiscard]] const settings::Integrations& current() const noexcept {
        return current_;
    }
    /// Whether the settings file holds the player's choices; until it does, the first-run chooser
    /// asks.
    [[nodiscard]] bool chosen() const noexcept {
        return preferences_.values().integrations.has_value();
    }
    /// The choices as saved, or all off before any.
    [[nodiscard]] settings::IntegrationChoices choices() const {
        return preferences_.values().integrations.value_or(settings::IntegrationChoices{});
    }
    [[nodiscard]] bool loginSession() const noexcept {
        return loginSession_;
    }

    /// Keeps `choices` and puts them in force; true when what is in force changed.
    bool choose(const settings::IntegrationChoices& choices);

    /// The stores the catalog reads.
    [[nodiscard]] library::CatalogStores catalogStores() const noexcept;
    /// The mode the Steam client runs in, or nothing while Steam is off.
    [[nodiscard]] std::optional<steam::Client::Mode> steamMode() const noexcept;
    /// Whether openSU's own Steam runs in its Gamescope, which the shell guards the pads and the
    /// focus against (Managed).
    [[nodiscard]] bool guardsSteam() const noexcept {
        return current_.steam == settings::SteamUse::Managed;
    }

  private:
    Preferences& preferences_;
    bool loginSession_;
    settings::Integrations current_;
};

} // namespace opensu::app
