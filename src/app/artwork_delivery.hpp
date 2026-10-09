// artwork_delivery — hands what the artwork fetcher saved, and what the store already holds, to the
// shell and the sound player: covers, console cards, glyphs, dock icons and UI sounds. Keeps the
// fetcher working on the games on screen first.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "audio/sound_player.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

class ArtworkDelivery {
  public:
    /// `namesArrived` is called when the arcade names were downloaded, which re-title the library.
    ArtworkDelivery(ui::Shell& shell, audio::SoundPlayer& sounds, artwork::ArtworkStore& store,
                    artwork::ArtworkFetcher& fetcher, std::function<void()> namesArrived)
        : shell_{shell}, sounds_{sounds}, store_{store}, fetcher_{fetcher},
          namesArrived_{std::move(namesArrived)} {
    }

    /// Shows artwork and loads sounds the fetcher has downloaded. Main loop only.
    void service();
    /// Loads every UI sound the store holds. Main loop only, once the audio device is open.
    void loadStoredSounds();
    /// Gives the shell the dock icons the store holds. Needs no GL context.
    void loadStoredNavIcons();
    /// A new catalog was requested from the fetcher: its queue order starts over.
    void restart();

  private:
    ui::Shell& shell_;
    audio::SoundPlayer& sounds_;
    artwork::ArtworkStore& store_;
    artwork::ArtworkFetcher& fetcher_;
    std::function<void()> namesArrived_;
    /// The games whose art the shell wants first, as last sent to the fetcher.
    std::vector<std::string> prioritised_;
};

} // namespace opensu::app
