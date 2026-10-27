// first_run_chooser — the first-run card: shown whenever the settings file holds no integration
// choices (a new install, and once for an install from before them), what openSU can work with
// besides the ROM library and the libretro runner, each a switch that is off until the player
// turns it on, and Done, which keeps the choices and puts them in force. There is no way past it
// but Done, and nothing is chosen for the player.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "integrations_in_force.hpp"
#include "ui/row_widgets.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

/// The id of the card's Done button.
inline constexpr std::string_view firstRunDoneId = "firstRun.done";

class FirstRunChooser final : public ui::RowActions {
  public:
    /// The choices were kept: what is in force changed.
    using Chosen = std::function<void()>;

    /// The card goes on `navigator`'s stack.
    FirstRunChooser(gpu2d::ui::Navigator& navigator, IntegrationsInForce& integrations,
                    audio::SdlSoundPlayer& sounds, Chosen chosen)
        : navigator_{navigator}, integrations_{integrations}, sounds_{sounds},
          chosen_{std::move(chosen)} {
    }

    /// Opens the card when no choice is saved yet.
    void openIfUnchosen();
    [[nodiscard]] bool active() const;

    // What the screen asks.
    /// A switch per integration, as the player has set them so far.
    [[nodiscard]] const std::vector<ui::SettingsRow>& rows() const noexcept {
        return rows_;
    }
    /// Flips switch `id`.
    void run(const std::string& id, int step) override;
    /// Keeps the choices, puts them in force and closes the card.
    void done();

  private:
    gpu2d::ui::Navigator& navigator_;
    IntegrationsInForce& integrations_;
    audio::SdlSoundPlayer& sounds_;
    Chosen chosen_;
    std::vector<ui::SettingsRow> rows_;
};

} // namespace opensu::app
