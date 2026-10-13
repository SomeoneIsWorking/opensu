// plugin_service — the plugins as the shell runs them: which ones run (a bundled plugin unless
// the player turned it off; a plugin the player added only once its exact permissions were
// accepted), the refreshes asked of them when the library changes or a game ends, and their play
// time merged into the library. Off the loop the plugins run on their own threads; this hands the
// loop what they reported.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "library/game.hpp"
#include "library/play_time.hpp"
#include "plugin/plugin_host.hpp"
#include "preferences.hpp"

namespace opensu::app {

class PluginService {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a failure.
        std::function<void(const std::string& text, bool error)> say;
        /// The play time of some game changed: apply it and show it.
        std::function<void()> playTimeChanged;
        /// A plugin's state changed: the Settings screen shows it again.
        std::function<void()> statusChanged;
    };

    /// Reads every plugin and starts the ones that are on.
    PluginService(const config::Config& environment, Preferences& preferences, Hooks hooks);

    /// Asks the running plugins to read again for `games`.
    void request(const std::vector<library::Game>& games);
    /// Takes what the plugins reported and tells the hooks. Main loop only.
    void service();
    /// Gives the ROM games in `games` the play time the plugins report.
    void apply(std::vector<library::Game>& games) const {
        reports_.apply(games);
    }

    [[nodiscard]] const std::vector<plugin::PluginInfo>& plugins() const noexcept {
        return host_.plugins();
    }
    /// Whether the player has `info` on.
    [[nodiscard]] bool enabled(const plugin::PluginInfo& info) const;
    /// Whether `info` is a plugin the player added whose permissions were not accepted as they are.
    [[nodiscard]] bool needsConsent(const plugin::PluginInfo& info) const;
    /// Turns the plugin with `id` on, its permissions accepted, or off; keeps the choice.
    void setEnabled(const std::string& id, bool on);

  private:
    Preferences& preferences_;
    Hooks hooks_;
    plugin::PluginHost host_;
    library::PlayTimeReports reports_;
    /// A plugin was turned off since the last `service`: its play time and state changed.
    bool turnedOff_{false};
};

} // namespace opensu::app
