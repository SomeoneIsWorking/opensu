// settings — what the player chose and opensu keeps between runs, in one JSON file under the
// config directory (`config::Config::configDir`).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "library/library_query.hpp"
#include "library/play_history.hpp"
#include "library/sections.hpp"

namespace opensu::settings {

/// iiSU's icon size level bounds and default (`xmbIconSizeLevel`, navigation.md §2.2).
inline constexpr int minIconSize = 1;
inline constexpr int maxIconSize = 20;
inline constexpr int defaultIconSize = 9;

struct Settings {
    /// How Library lays out its folders (iiSU `romCategoryLayoutMode`).
    library::LibraryMode libraryMode{library::LibraryMode::Standard};

    /// Whether the dock stays up on Library (iiSU `persistentNavBarOnPlatforms`, which iiSU
    /// defaults to off; openSU defaults to on so the way back to Home stays visible).
    bool pinLibraryDock{true};

    /// How large Library's tiles are, 1 to 20 (iiSU `xmbIconSizeLevel`).
    int iconSize{defaultIconSize};

    /// The sort, installed-only and source filters. The search text and the hidden-only filter
    /// are for one run and are not kept.
    library::ViewOptions view;

    /// The games the player hid.
    library::HiddenGames hidden;

    /// When each game was last launched from opensu.
    library::PlayHistory lastPlayed;

    bool operator==(const Settings&) const = default;
};

class Store {
  public:
    explicit Store(std::filesystem::path file);

    /// The saved settings. A file that is missing gives the defaults; one that cannot be read or
    /// parsed, or a value that is not one of the choices, is reported and takes the default.
    [[nodiscard]] Settings load() const;

    /// Replaces the file whole. False with `error` when it cannot; the old file is then kept.
    [[nodiscard]] bool save(const Settings& settings, std::string& error) const;

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }

  private:
    std::filesystem::path file_;
};

} // namespace opensu::settings
