// interface_sounds — puts the player's choice of interface sounds into the sound player: one of
// openSU's sets from the assets, or iiSU's from the release placed in the iiSU folder
// (`IisuInstall`) over openSU's default set (`settings::shippedSet`), which keeps any effect iiSU's
// file lacks.
#pragma once

#include <filesystem>
#include <memory>

#include "artwork/iisu_install.hpp"
#include "audio/sdl_sound_player.hpp"
#include "settings/settings.hpp"

namespace opensu::app {

class InterfaceSounds {
  public:
    /// openSU's sets are read from `assetsDir`, iiSU's from `iisu`.
    InterfaceSounds(audio::SdlSoundPlayer& sounds, std::filesystem::path assetsDir,
                    const artwork::IisuInstall& iisu)
        : sounds_{sounds}, assetsDir_{std::move(assetsDir)}, iisu_{iisu} {
    }

    /// Loads the sounds `chosen` asks for, unless they are the ones loaded. Main loop only.
    void apply(const settings::Settings& chosen);

  private:
    audio::SdlSoundPlayer& sounds_;
    std::filesystem::path assetsDir_;
    const artwork::IisuInstall& iisu_;
    /// What is loaded: which set, and from which iiSU file.
    bool loaded_{false};
    settings::SoundSet chosen_{settings::SoundSet::Marimba};
    std::shared_ptr<const artwork::IisuPack> pack_;
};

} // namespace opensu::app
