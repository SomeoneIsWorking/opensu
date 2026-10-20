// integration_settings — Settings > Integrations: ROMs and libretro as always on, a switch for each
// opt-in integration, Steam's mode while Steam is on (Normal and Managed on a desktop, Managed
// alone in a login session), and Isolated as not available yet. A change is kept and applied at
// once.
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "integrations_in_force.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class IntegrationSettings {
  public:
    /// The choices changed: what is in force is applied.
    using Changed = std::function<void()>;

    IntegrationSettings(IntegrationsInForce& integrations, Changed changed)
        : integrations_{integrations}, changed_{std::move(changed)} {
    }

    /// The category's id; its rows' ids start with it and a dot.
    static constexpr std::string_view categoryId = "integrations";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: A flips a switch; A, Left and Right step Steam's mode.
    void run(const std::string& id, int step);

  private:
    IntegrationsInForce& integrations_;
    Changed changed_;
};

} // namespace opensu::app
