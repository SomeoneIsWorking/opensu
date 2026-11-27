// steam — reads a local Steam installation: which apps exist, whether they are
// installed, what artwork exists, and when they were last played.
//
// It never talks to the Steam client and never writes.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fumarole_installs.hpp"
#include "game.hpp"

namespace opensu::library::steam {

/// One place Steam keeps games.
struct LibraryFolder {
    std::filesystem::path path;
    /// Identifies the physical library. Steam assigns the same value to two
    /// mount points of one drive, which would otherwise duplicate every game.
    std::string contentId;
    /// The apps Steam records in it, app id to bytes on disk; empty in the legacy layout.
    std::map<std::string, std::uint64_t> apps;
};

/// Reads a local Steam installation.
class Library {
  public:
    /// Finds the usable install roots. When `explicit` is non-empty it is
    /// authoritative — the caller asked for exactly those — otherwise the
    /// standard locations are probed.
    [[nodiscard]] static Library discover(const std::filesystem::path& home,
                                          const std::vector<std::filesystem::path>& explicitRoots);

    /// The install roots in use.
    [[nodiscard]] const std::vector<std::filesystem::path>& roots() const noexcept {
        return roots_;
    }

    /// Every library folder, across all roots.
    [[nodiscard]] std::vector<LibraryFolder> libraryFolders() const;

    /// Lists every app the installation knows about, installed or not.
    [[nodiscard]] std::vector<Game> list() const;

    /// True when the app's manifest says it is fully installed with no update pending.
    [[nodiscard]] bool installed(std::string_view appId) const;

    /// The app's manifest in whichever library holds it; nothing once Steam has removed the app.
    [[nodiscard]] std::optional<std::filesystem::path> manifestOf(std::string_view appId) const;

  private:
    /// The manifest's StateFlags; 0 when there is none.
    [[nodiscard]] long long stateFlags(std::string_view appId) const;

    std::vector<std::filesystem::path> roots_;
};

/// A Provider over a discovered installation and fumarole's installs: a game Steam has installed
/// stays Steam's, one only fumarole installed starts through fumarole. With no Steam here, the
/// account's other owned games fumarole can install are listed too, not installed (asked once).
/// Throws when there is neither Steam nor fumarole, which the catalog reports as a problem rather
/// than a failure, and when, with no Steam, fumarole cannot list the account's games (not signed
/// in), which the catalog reports as needing the player.
class Provider final : public library::Provider {
  public:
    Provider(Library library, fumarole::Installs fumarole);

    [[nodiscard]] Source source() const override {
        return Source::Steam;
    }
    [[nodiscard]] std::vector<Game> list(const std::stop_token& stop) override;

  private:
    Library library_;
    fumarole::Installs fumarole_;
    /// fumarole's owned games, asked on the first listing without Steam.
    std::optional<std::vector<Game>> owned_;
};

} // namespace opensu::library::steam