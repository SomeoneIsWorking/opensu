// wine_settings — Settings › Wine: the runner Windows GOG and Epic games run under (Automatic or
// one found), the shared prefix and how many games have their own, and Steam left to its own
// compatibility tools. Also prepares a launch with the runner and prefix the player chose.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "config/config.hpp"
#include "library/game.hpp"
#include "library/wine.hpp"
#include "preferences.hpp"
#include "settings/integrations.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class WineSettings {
  public:
    /// `config` gives the data dir, PATH and the Steam roots searched for runners, which are
    /// searched only while Steam is in force in `integrations`.
    WineSettings(Preferences& preferences, const config::Config& config,
                 const settings::Integrations& integrations)
        : preferences_{preferences}, config_{config}, integrations_{integrations} {
    }

    static constexpr std::string_view categoryId = "wine";

    /// Looks for runners again.
    void rescan();
    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: the runner row steps through Automatic and the runners found.
    void run(const std::string& id, int step);

    /// `game` with the Wine runner and prefix added when it is a Windows GOG or Epic install
    /// (`library::wine::prepare`); why not otherwise, else empty.
    [[nodiscard]] std::string prepare(library::Game& game) const;

  private:
    [[nodiscard]] library::wine::Search search() const;

    Preferences& preferences_;
    const config::Config& config_;
    const settings::Integrations& integrations_;
    std::vector<library::wine::Runner> found_;
};

} // namespace opensu::app
