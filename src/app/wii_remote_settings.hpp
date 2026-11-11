// wii_remote_settings — Settings › Controls › Wii Remotes: whether Wii games on Dolphin play the
// real remotes when they are here (`settings::Settings::realWiiRemotes`, read into the launch as
// `library::LaunchSetup::realWiiRemotes`), and the pointer fitted to the screen. Fitting listens
// for a remote pointing at the screen's top-left corner, then its bottom-right, A pressed at each
// (`gamepad::WiiPointerRange::fromCorners`): meanwhile the remotes read the camera's whole view and
// their pointer button comes here instead of pressing in the shell; running the row again stops
// it. The range found is saved, said with where the sensor bar stands, and set on the pads, as the
// saved one is at start.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gamepad/event.hpp"
#include "gamepad/pad_source.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class WiiRemoteSettings {
  public:
    /// Sets the saved range on `pads`; `changed` runs when the rows read differently.
    WiiRemoteSettings(Preferences& preferences, gamepad::PadSource& pads,
                      std::function<void()> changed);

    /// The Controls page's rows: a heading, the remotes' use in Wii games and the pointer.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Steps the choice, or starts or stops fitting the pointer.
    void run(const std::string& id, int step);

    /// Takes a pad's pointer event while fitting; false for every other event, and always when
    /// not fitting.
    [[nodiscard]] bool take(const gamepad::Event& event);
    [[nodiscard]] bool fitting() const noexcept {
        return corner_ != Corner::None;
    }

    /// Whether a Wii game plays the real remotes when they are here.
    [[nodiscard]] bool realRemotes() const;

  private:
    enum class Corner : std::uint8_t {
        None,
        TopLeft,
        BottomRight,
    };

    /// Stops fitting; the pads read the saved range again.
    void stop();
    [[nodiscard]] gamepad::WiiPointerRange saved() const;

    Preferences& preferences_;
    gamepad::PadSource& pads_;
    std::function<void()> changed_;
    Corner corner_{Corner::None};
    /// Where each remote aims in its camera's view now, by `Event::source`.
    std::map<std::string, std::pair<float, float>> aims_;
    std::pair<float, float> topLeft_{};
};

} // namespace opensu::app
