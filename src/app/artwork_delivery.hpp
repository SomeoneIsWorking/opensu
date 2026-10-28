// artwork_delivery — hands what the artwork fetcher saved, and what the store already holds, to the
// shell and the sound player: covers and UI sounds. Keeps the
// fetcher working on the games on screen first.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "audio/sdl_sound_player.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

class ArtworkDelivery {
  public:
    struct Hooks {
        /// The arcade names were downloaded, which re-title the library.
        std::function<void()> namesArrived;
        /// Game `id`'s cover was saved as `file`; the hook shows it.
        std::function<void(const std::string& id, const std::filesystem::path& file)> coverArrived;
        /// Game `id`'s download ended without a cover: its source has none, or the round ended.
        std::function<void(const std::string& id)> coverMissing;
    };

    ArtworkDelivery(ui::Shell& shell, audio::SdlSoundPlayer& sounds, artwork::ArtworkStore& store,
                    artwork::ArtworkFetcher& fetcher, Hooks hooks)
        : shell_{shell}, sounds_{sounds}, store_{store}, fetcher_{fetcher},
          hooks_{std::move(hooks)} {
    }

    /// Shows artwork and loads sounds the fetcher has downloaded. Main loop only.
    void service();
    /// Loads every UI sound the store holds. Main loop only, once the audio device is open.
    void loadStoredSounds();
    /// A new catalog was requested from the fetcher: its queue order starts over.
    void restart();

  private:
    ui::Shell& shell_;
    audio::SdlSoundPlayer& sounds_;
    artwork::ArtworkStore& store_;
    artwork::ArtworkFetcher& fetcher_;
    Hooks hooks_;
    /// The games whose art the shell wants first, as last sent to the fetcher.
    std::vector<std::string> prioritised_;
};

} // namespace opensu::app
