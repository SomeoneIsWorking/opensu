// interface_sounds — puts the player's choice of interface sounds into the sound player: openSU's
// own set from the assets, with iiSU's from the release placed in the iiSU folder (`IisuInstall`)
// over it when chosen. An effect iiSU's file lacks keeps openSU's.
#pragma once

#include <filesystem>
#include <memory>

#include "artwork/iisu_install.hpp"
#include "audio/sdl_sound_player.hpp"
#include "settings/settings.hpp"

namespace opensu::app {

class InterfaceSounds {
  public:
    /// openSU's set is read from `assetsDir`, iiSU's from `iisu`.
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
    /// What is loaded: whose, and from which iiSU file.
    bool loaded_{false};
    settings::AssetOwner owner_{settings::AssetOwner::OpenSu};
    std::shared_ptr<const artwork::IisuPack> pack_;
};

} // namespace opensu::app
