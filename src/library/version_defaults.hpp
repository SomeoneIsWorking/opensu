// version_defaults — the version the player made their game's default, by version id, so a rescan
// keeps it; `apply` marks the catalog's games for `TitleCopies`.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library {

class VersionDefaults {
  public:
    VersionDefaults() = default;
    explicit VersionDefaults(std::set<std::string> ids) : ids_{std::move(ids)} {
    }

    /// Sets each game's `versionDefault`.
    void apply(std::vector<Game>& games) const;
    /// Makes version `id` the default of its game, whose versions are `game`.
    void makeDefault(const std::string& id, const std::vector<std::string>& game);

    [[nodiscard]] const std::set<std::string>& ids() const noexcept {
        return ids_;
    }

    bool operator==(const VersionDefaults&) const = default;

  private:
    std::set<std::string> ids_;
};

} // namespace opensu::library
