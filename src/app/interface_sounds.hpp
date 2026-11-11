// interface_sounds — puts the interface sounds into the sound player: the player's choice, one of
// openSU's sets from the assets or iiSU's from the release placed in the iiSU folder
// (`IisuInstall`) over openSU's default set (`settings::shippedSet`), which keeps any effect iiSU's
// file lacks; or, with no choice, the interface theme's: its set, and its own files over it.
#pragma once

#include <filesystem>
#include <memory>

#include "artwork/iisu_install.hpp"
#include "audio/sdl_sound_player.hpp"
#include "settings/settings.hpp"
#include "theme/interface_theme.hpp"

namespace opensu::app {

class InterfaceSounds {
  public:
    /// openSU's sets are read from `assetsDir`, iiSU's from `iisu`.
    InterfaceSounds(audio::SdlSoundPlayer& sounds, std::filesystem::path assetsDir,
                    const artwork::IisuInstall& iisu)
        : sounds_{sounds}, assetsDir_{std::move(assetsDir)}, iisu_{iisu} {
    }

    /// Loads the sounds `chosen` asks for, else `theme`'s, unless they are the ones loaded. Main
    /// loop only.
    void apply(const settings::Settings& chosen,
               const std::shared_ptr<const theme::InterfaceTheme>& theme);

  private:
    /// Loads each effect file `theme`'s sounds folder holds over the set loaded.
    void loadTheme(const theme::InterfaceTheme& theme);

    audio::SdlSoundPlayer& sounds_;
    std::filesystem::path assetsDir_;
    const artwork::IisuInstall& iisu_;
    /// What is loaded: which set, from which iiSU file, and which theme's files over it.
    bool loaded_{false};
    settings::SoundSet chosen_{settings::SoundSet::Marimba};
    std::shared_ptr<const artwork::IisuPack> pack_;
    std::shared_ptr<const theme::InterfaceTheme> theme_;
};

} // namespace opensu::app
