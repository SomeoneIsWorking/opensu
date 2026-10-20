// first_run_chooser — the first-run card's buttons: shown whenever the settings file holds no
// integration choices (a new install, and once for an install from before them), every option off
// until the player turns it on; A toggles the focused option, A on Done keeps the choices and puts
// them in force. There is no way past it but Done, and nothing is chosen for the player.
#pragma once

#include <functional>

#include "audio/sdl_sound_player.hpp"
#include "gamepad/event.hpp"
#include "integrations_in_force.hpp"
#include "ui/integration_chooser.hpp"

namespace opensu::app {

class FirstRunChooser {
  public:
    /// The choices were kept: what is in force changed.
    using Chosen = std::function<void()>;

    FirstRunChooser(ui::IntegrationChooser& card, IntegrationsInForce& integrations,
                    audio::SdlSoundPlayer& sounds, Chosen chosen)
        : card_{card}, integrations_{integrations}, sounds_{sounds}, chosen_{std::move(chosen)} {
    }

    /// Opens the card when no choice is saved yet.
    void openIfUnchosen();
    [[nodiscard]] bool active() const noexcept {
        return card_.isOpen();
    }
    /// A button while the card is up.
    void act(gamepad::Button button);

  private:
    void done();

    ui::IntegrationChooser& card_;
    IntegrationsInForce& integrations_;
    audio::SdlSoundPlayer& sounds_;
    Chosen chosen_;
};

} // namespace opensu::app
