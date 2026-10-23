// integration_settings — Settings > Integrations: ROMs and libretro as always on, a switch for each
// opt-in integration, Steam's mode while Steam is on and the build's client has more than one
// (Normal, Managed and Isolated on a Linux desktop; no Normal in a login session) and, for
// Isolated, which controllers its sandbox gives Steam. Edits are staged and kept by the Apply row.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "integrations_in_force.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class IntegrationSettings {
  public:
    /// The staged choices were applied: what is in force is put in effect.
    using Changed = std::function<void()>;

    IntegrationSettings(IntegrationsInForce& integrations, Changed changed)
        : integrations_{integrations}, changed_{std::move(changed)} {
    }

    /// The category's id; its rows' ids start with it and a dot.
    static constexpr std::string_view categoryId = "integrations";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: A flips a switch or applies; Left and Right step a choice.
    void run(const std::string& id, int step);
    /// Drops anything staged and shows the saved choices again, when the screen is opened.
    void sync() noexcept {
        pending_.reset();
    }

  private:
    /// The choices the page shows: the staged edit, or the saved ones.
    [[nodiscard]] settings::IntegrationChoices effective() const {
        return pending_.value_or(integrations_.choices());
    }

    IntegrationsInForce& integrations_;
    Changed changed_;
    /// The edit not yet applied; nothing until a row is changed.
    std::optional<settings::IntegrationChoices> pending_;
};

} // namespace opensu::app
