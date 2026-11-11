// menu_music — the interface theme's loop behind Home and the menus (Settings › Audio › Menu
// music): decoded the first time it is wanted for a theme, played while the switch and the UI
// sounds are on and no game runs, stopped otherwise.
#pragma once

#include <memory>

#include "audio/sdl_sound_player.hpp"
#include "settings/settings.hpp"
#include "theme/interface_theme.hpp"

namespace opensu::app {

class MenuMusic {
  public:
    /// Plays on `sounds`' device, which must outlive it.
    explicit MenuMusic(audio::SdlSoundPlayer& sounds) : sounds_{sounds} {
    }

    /// Whether the music is wanted: `chosen` asks for it and for the UI sounds, and no game runs.
    [[nodiscard]] static bool wanted(const settings::Settings& chosen, bool inGame) noexcept {
        return chosen.menuMusic && chosen.uiSounds && !inGame;
    }

    /// Once a frame: loads `theme`'s loop when it is wanted and not yet loaded, and plays or stops
    /// it. Main loop only.
    void service(const std::shared_ptr<const theme::InterfaceTheme>& theme, bool wanted);

  private:
    audio::SdlSoundPlayer& sounds_;
    /// The theme the loop was loaded for, also when it has none or it would not load.
    std::shared_ptr<const theme::InterfaceTheme> theme_;
    std::unique_ptr<audio::SoundLoop> loop_;
};

} // namespace opensu::app
