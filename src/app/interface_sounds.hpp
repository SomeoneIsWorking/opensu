// interface_sounds — puts the player's choice of interface sounds into the sound player: openSU's
// own set from the assets, with iiSU's from the player's iiSU release (through IisuPack) over it
// when chosen. An effect iiSU's file lacks keeps openSU's.
#pragma once

#include <filesystem>

#include "audio/sdl_sound_player.hpp"
#include "settings/settings.hpp"

namespace opensu::app {

class InterfaceSounds {
  public:
    /// openSU's set is read from `assetsDir`.
    InterfaceSounds(audio::SdlSoundPlayer& sounds, std::filesystem::path assetsDir)
        : sounds_{sounds}, assetsDir_{std::move(assetsDir)} {
    }

    /// Loads the sounds `chosen` asks for, unless they are the ones loaded. Main loop only.
    void apply(const settings::Settings& chosen);

  private:
    audio::SdlSoundPlayer& sounds_;
    std::filesystem::path assetsDir_;
    /// What is loaded: whose, and from which iiSU file.
    bool loaded_{false};
    settings::AssetOwner owner_{settings::AssetOwner::OpenSu};
    std::filesystem::path file_;
};

} // namespace opensu::app
