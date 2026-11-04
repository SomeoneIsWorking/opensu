// shared_saves — a game's shared saves on its details page: the Saves fact from how they stand
// with each target (`SaveSharing::standings`: In sync with Desk, Newer on WebDAV, Conflict with
// the saves folder, Desk is offline, ...), its row (Sync saves now, or Pick which save to keep in
// a conflict) and the pick, a dialog with each side's date and size whose answer
// `SaveSharing::keep` carries out, the replaced saves backed up first.
#pragma once

#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "library/game.hpp"
#include "save_sharing.hpp"

namespace opensu::app {

class SharedSaves {
  public:
    SharedSaves(SaveSharing& saves, gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds)
        : saves_{saves}, navigator_{navigator}, sounds_{sounds} {
    }

    /// What the details page shows of `game`'s shared saves.
    struct View {
        /// The Saves fact; empty while there is no target.
        std::string fact;
        /// The row's label; empty with the fact.
        std::string action;
    };
    [[nodiscard]] View view(const library::Game& game) const;
    /// The row: syncs now, or asks which save to keep while two conflict.
    void act(const library::Game& game);

  private:
    SaveSharing& saves_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
};

} // namespace opensu::app
