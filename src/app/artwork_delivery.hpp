// artwork_delivery — hands the covers the artwork fetcher saved to the shell. Keeps the fetcher
// working on the games on screen first.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "artwork_fetcher.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

class ArtworkDelivery {
  public:
    struct Hooks {
        /// The arcade names or an emulator's icon arrived, which the next catalog read gives the
        /// games.
        std::function<void()> libraryStale;
        /// Game `id`'s cover was saved as `file`; the hook shows it.
        std::function<void(const std::string& id, const std::filesystem::path& file)> coverArrived;
        /// Game `id`'s download ended without a cover: its source has none, or the round ended.
        std::function<void(const std::string& id)> coverMissing;
    };

    ArtworkDelivery(ui::Shell& shell, artwork::ArtworkFetcher& fetcher, Hooks hooks)
        : shell_{shell}, fetcher_{fetcher}, hooks_{std::move(hooks)} {
    }

    /// Shows the covers the fetcher has downloaded. Main loop only.
    void service();
    /// A new catalog was requested from the fetcher: its queue order starts over.
    void restart();

  private:
    ui::Shell& shell_;
    artwork::ArtworkFetcher& fetcher_;
    Hooks hooks_;
    /// The games whose art the shell wants first, as last sent to the fetcher.
    std::vector<std::string> prioritised_;
};

} // namespace opensu::app
