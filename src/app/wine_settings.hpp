// wine_settings — Settings › Wine: the runner Windows GOG and Epic games run under (GE-Proton kept
// up to date by default, one GE-Proton build, another runner found, or system Wine), every runner
// and runtime with where it lives, its size and the games using it (a page per runtime lists them
// and pins a game to it), the GE-Proton check (`WineRunners`) and a test run, the shared prefix and
// how many games have their own. Also prepares a launch with the runner the game resolves to, the
// one rule the counts use, and holds a launch while its GE-Proton downloads.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/config.hpp"
#include "library/game.hpp"
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

    /// A launch readied for its runner.
    struct Prepared {
        /// Why it cannot run; empty when it can.
        std::string refusal;
        /// What the launch's environment gets.
        std::vector<std::string> environment;
        /// Held until GE-Proton is installed (`serviceHeld`).
        bool waiting{false};
    };

    /// The launch held for a runner download, as the launch panel shows it.
    struct Held {
        std::string line;
        std::optional<double> fraction;
        /// The game, once it can start.
        std::optional<library::Game> ready;
        /// It cannot: the download failed or the player cancelled.
        bool failed{false};
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
    /// (`library::wine::prepare`); held while the GE-Proton it needs downloads.
    [[nodiscard]] Prepared prepare(library::Game& game);
    /// The held launch's state; nothing when none is held. Main loop only.
    [[nodiscard]] std::optional<Held> serviceHeld();
    /// Drops the held launch; false when none was held.
    bool dropHeld();

    /// The GE-Proton builds a rotation must keep and why: a running game's, the player's pick,
    /// the games pinned to one.
    [[nodiscard]] WineRunners::InUse inUse() const;

  private:
    [[nodiscard]] library::wine::Search search() const;
    [[nodiscard]] library::wine::Choice choice() const;
    /// The Windows GOG and Epic installs.
    [[nodiscard]] std::vector<library::Game> windowsGames() const;
    /// The games whose runner resolves to one with `wine`; with an empty `wine`, those on any
    /// Proton build.
    [[nodiscard]] std::vector<library::Game> gamesOn(const std::filesystem::path& wine) const;
    void stepRunner(int step);
    void test();

    Preferences& preferences_;
    const config::Config& config_;
    const settings::Integrations& integrations_;
    WineRunners& runners_;
    Hooks hooks_;
    std::vector<library::wine::Runner> found_;
    /// The Proton build the last launch ran under.
    std::filesystem::path launched_;
    std::optional<library::Game> held_;
    bool heldChecking_{false};
    bool heldDropped_{false};
};

} // namespace opensu::app
