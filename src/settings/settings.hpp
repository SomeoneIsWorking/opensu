// settings — what the player chose and opensu keeps between runs, in one JSON file under the
// config directory (`config::Config::configDir`).
#pragma once

#include <filesystem>
#include <string>

#include "library/sections.hpp"

namespace opensu::settings {

struct Settings {
    /// How Library lays out its folders (iiSU `romCategoryLayoutMode`).
    library::LibraryMode libraryMode{library::LibraryMode::Standard};

    /// Whether the dock stays up on Library (iiSU `persistentNavBarOnPlatforms`, which iiSU
    /// defaults to off; openSU defaults to on so the way back to Home stays visible).
    bool pinLibraryDock{true};

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
