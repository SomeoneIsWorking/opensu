// copy_places — where a paired openSU's install goes when Copy to this device copies it here, and
// whether this openSU can take it over: a Steam game into fumarole's apps folder (fumarole here),
// a GOG game under gogdl's folder for it, an Epic game in legendary's install folder, a
// games-folder game in the first games folder; each under the host's folder name. A build for
// another OS is refused, a Windows build only where Wine runs.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lan/wire.hpp"
#include "library/game.hpp"

namespace opensu::app {

class CopyPlaces {
  public:
    struct Places {
        /// fumarole's apps folder; empty when fumarole is not here.
        std::filesystem::path fumaroleApps;
        /// openSU's data directory, under which gogdl's games are.
        std::filesystem::path dataDir;
        /// legendary's install folder when the player set none.
        std::filesystem::path epicDefault;
        /// The folder the player set for a store's installs; nothing for the store's own.
        std::function<std::optional<std::filesystem::path>(library::Source)> storeFolder;
        /// The player's games folders.
        std::function<std::vector<std::filesystem::path>()> gameFolders;
    };

    explicit CopyPlaces(Places places) : places_{std::move(places)} {
    }

    /// Whether copies of a game of `store` (`library::key`) can be taken here at all.
    [[nodiscard]] bool takes(std::string_view store) const;
    /// The folder `install` is copied into here; nothing, with `why`, when it cannot be taken.
    [[nodiscard]] std::optional<std::filesystem::path>
    folderFor(const lan::wire::InstallCopy& install, std::string& why) const;

  private:
    Places places_;
};

} // namespace opensu::app
