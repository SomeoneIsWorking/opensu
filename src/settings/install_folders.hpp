// install_folders — where games install: one default folder and an optional folder per store. The
// one resolver every store's install flow reads its destination from.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "library/game.hpp"

namespace opensu::settings {

/// The stores opensu installs from.
inline constexpr std::array installStores{library::Source::Steam, library::Source::Epic,
                                          library::Source::Gog};

/// Where each store installs when openSU names no folder, as far as it is known here.
using StoreFolders = std::map<library::Source, std::filesystem::path>;

/// Which rule a store's folder comes from.
enum class FolderOrigin : std::uint8_t {
    /// The folder the player set for that store.
    Store,
    /// The default folder the player set for every store.
    Default,
    /// Nothing set: the store's own choice.
    StoreOwn,
};

/// Where a store installs and why there.
struct Destination {
    /// Empty when the store's own choice is not known here.
    std::filesystem::path folder;
    FolderOrigin origin{FolderOrigin::StoreOwn};

    bool operator==(const Destination&) const = default;
};

class InstallFolders {
  public:
    /// The folder every store installs into unless it has its own; empty for none.
    [[nodiscard]] const std::filesystem::path& defaultFolder() const noexcept {
        return default_;
    }
    void setDefault(std::filesystem::path folder) {
        default_ = std::move(folder);
    }

    /// The store's own folder; empty when it follows the default.
    [[nodiscard]] std::filesystem::path storeFolder(library::Source store) const;
    /// Sets the store's own folder; an empty `folder` makes the store follow the default again.
    void setStore(library::Source store, std::filesystem::path folder);

    /// Where `store` installs: its own folder, else the default, else nothing, which leaves the
    /// store's own choice.
    [[nodiscard]] std::optional<std::filesystem::path> effective(library::Source store) const;

    /// Where `store` installs, as the player reads it: `effective`, else the store's own folder
    /// from `own`.
    [[nodiscard]] Destination resolve(library::Source store, const StoreFolders& own) const;

    bool operator==(const InstallFolders&) const = default;

  private:
    std::filesystem::path default_;
    std::map<library::Source, std::filesystem::path> stores_;
};

/// What a folder must allow.
enum class Need : std::uint8_t { Read, Write };

/// Why `folder` cannot be used: it is not absolute, does not exist, is not a folder or cannot be
/// read (or, for `Write`, written). Empty when it can.
[[nodiscard]] std::string refusal(const std::filesystem::path& folder, Need need = Need::Write);

} // namespace opensu::settings
