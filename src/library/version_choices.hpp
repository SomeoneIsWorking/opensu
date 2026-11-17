// version_choices — what the player changed about how games group into versions (`titles`): a
// version grouped with another, one kept apart, the default of its game, those moved to Extras. All
// by version id, so a rescan keeps them; `apply` marks the catalog's games for `gameTitles`.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "game.hpp"

namespace opensu::library {

class VersionChoices {
  public:
    /// A version id to the id of the version the player grouped it with.
    using Joins = std::map<std::string, std::string>;

    VersionChoices() = default;
    VersionChoices(Joins with, std::set<std::string> apart, std::set<std::string> defaults,
                   std::set<std::string> extras)
        : with_{std::move(with)}, apart_{std::move(apart)}, defaults_{std::move(defaults)},
          extras_{std::move(extras)} {
    }

    /// Sets each game's `versionWith`, `versionApart`, `versionDefault` and `inExtras`.
    void apply(std::vector<Game>& games) const;

    /// Groups version `id` with version `other`'s game.
    void groupWith(const std::string& id, const std::string& other);
    /// Takes version `id` out of its game into one of its own; the versions grouped with it go
    /// back to where their titles put them.
    void separate(const std::string& id);
    /// Makes version `id` the default of its game, whose versions are `game`.
    void makeDefault(const std::string& id, const std::vector<std::string>& game);
    /// Moves version `id` to its game's Extras or back.
    void setExtra(const std::string& id, bool extra);

    [[nodiscard]] const Joins& with() const noexcept {
        return with_;
    }
    [[nodiscard]] const std::set<std::string>& apart() const noexcept {
        return apart_;
    }
    [[nodiscard]] const std::set<std::string>& defaults() const noexcept {
        return defaults_;
    }
    [[nodiscard]] const std::set<std::string>& extras() const noexcept {
        return extras_;
    }

    bool operator==(const VersionChoices&) const = default;

  private:
    Joins with_;
    std::set<std::string> apart_;
    std::set<std::string> defaults_;
    std::set<std::string> extras_;
};

} // namespace opensu::library
