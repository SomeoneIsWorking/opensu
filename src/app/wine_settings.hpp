// wine_settings — Settings › Wine: the runner Windows GOG and Epic games run under (Automatic, with
// what it resolves to now and why, or any runner found), every runner and runtime with where it
// lives, its size and the games using it by store (Steam's by the tool picked in Steam; a page per
// runtime lists them and pins a GOG or Epic game to it), whether openSU keeps GE-Proton up to
// date, the GE-Proton check (`WineRunners`) and a test run, the shared prefix and how many games
// have their own. Also prepares a launch with the runner the game resolves to now
// (`library::wine::resolve`, the one rule the counts use); a launch never waits for a download.
#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/config.hpp"
#include "library/game.hpp"
#include "library/steam.hpp"
#include "library/wine.hpp"
#include "preferences.hpp"
#include "settings/integrations.hpp"
#include "ui/setting_row.hpp"
#include "wine_runners.hpp"

namespace opensu::app {

class WineSettings {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The library's games, for the counts and pins.
        std::function<std::vector<library::Game>()> games;
        /// Whether a launch runs now.
        std::function<bool()> gameRunning;
    };

    /// `config` gives the data dir, PATH and the Steam roots searched for runners, which are
    /// searched only while Steam is in force in `integrations`.
    WineSettings(Preferences& preferences, const config::Config& config,
                 const settings::Integrations& integrations, WineRunners& runners, Hooks hooks)
        : preferences_{preferences}, config_{config}, integrations_{integrations},
          runners_{runners}, hooks_{std::move(hooks)} {
    }

    static constexpr std::string_view categoryId = "wine";

    /// Looks for runners again.
    void rescan();
    [[nodiscard]] ui::SettingsCategory page() const;
    /// The page of one runtime: what it is and the games using it; nothing for another id.
    [[nodiscard]] std::optional<ui::SettingsCategory> subPage(const std::string& id) const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: the runner choice, Check now, Test, a game's pin.
    void run(const std::string& id, int step);

    /// `game` with its runner and prefix added when it is a Windows GOG or Epic install
    /// (`library::wine::prepare`).
    [[nodiscard]] library::wine::Prepared prepare(library::Game& game);

    /// The GE-Proton builds a rotation must keep and why: a running game's, the player's pick,
    /// the games pinned to one.
    [[nodiscard]] WineRunners::InUse inUse() const;

  private:
    /// Where runners are, with Steam's folders from `steam` when Steam is in force.
    [[nodiscard]] library::wine::Search search(const library::steam::Library* steam) const;
    [[nodiscard]] library::wine::Choice choice() const;
    /// The Windows GOG and Epic installs.
    [[nodiscard]] std::vector<library::Game> windowsGames() const;
    /// The games whose runner resolves to one with `wine`; with an empty `wine`, those on any
    /// Proton build.
    [[nodiscard]] std::vector<library::Game> gamesOn(const std::filesystem::path& wine) const;
    /// The installed Steam games Steam runs under `runner`.
    [[nodiscard]] std::vector<library::Game>
    steamGamesOn(const library::wine::Runner& runner) const;
    void stepRunner(int step);
    void test();

    Preferences& preferences_;
    const config::Config& config_;
    const settings::Integrations& integrations_;
    WineRunners& runners_;
    Hooks hooks_;
    std::vector<library::wine::Runner> found_;
    std::optional<library::wine::Runner> lutris_;
    std::filesystem::path umuRun_;
    /// The GAMEIDs Proton launches run under, read again with the runners.
    library::umu::Database umu_;
    /// The installed Steam games by the folder of the tool Steam runs them under.
    std::map<std::filesystem::path, std::vector<library::Game>> steamOn_;
    /// The Proton build the last launch ran under.
    std::filesystem::path launched_;
};

} // namespace opensu::app
