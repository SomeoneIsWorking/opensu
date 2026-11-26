// integration_settings — each added integration's own rows in Settings: Steam's mode where the
// build's client has more than one (Normal, Managed and Isolated on a Linux desktop; no Normal in a
// login session) and, for Isolated, which controllers its sandbox gives Steam, on the Steam page
// while Steam comes through the client; and a Remove row heading the pages of the integrations with
// no Library tile of their own (standalone emulators, Lua plugins, the LAN). A pick takes effect at
// once; Remove asks first. Adding is Library's Add page (`IntegrationWizard`).
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "integrations_in_force.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class IntegrationSettings {
  public:
    /// A choice was kept: what is in force is put in effect.
    using Changed = std::function<void()>;

    IntegrationSettings(IntegrationsInForce& integrations, gpu2d::ui::Navigator& navigator,
                        Changed changed)
        : integrations_{integrations}, navigator_{navigator}, changed_{std::move(changed)} {
    }

    /// Steam's rows: its mode where there is a choice, and Isolated's controllers; none through
    /// fumarole or while Steam is off.
    [[nodiscard]] std::vector<ui::SettingsRow> steamRows() const;
    /// The row removing the integration entry `entry` (`integrationEntries`).
    [[nodiscard]] static ui::SettingsRow removeRow(std::string_view entry);
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: a choice keeps the pick; Remove asks, then turns the integration off.
    void run(const std::string& id, int step);

  private:
    void keep(const settings::IntegrationChoices& choices);

    IntegrationsInForce& integrations_;
    gpu2d::ui::Navigator& navigator_;
    Changed changed_;
};

} // namespace opensu::app
