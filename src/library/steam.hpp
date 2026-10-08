// steam — reads a local Steam installation: which apps exist, whether they are
// installed, what artwork exists, and when they were last played.
//
// It never talks to the Steam client. The one thing it writes is an install request: an app
// manifest that tells Steam, when it next starts, to download the app.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace iideck::library::steam {

/// One place Steam keeps games.
struct LibraryFolder {
    std::filesystem::path path;
    /// Identifies the physical library. Steam assigns the same value to two
    /// mount points of one drive, which would otherwise duplicate every game.
    std::string contentId;
};

/// An update Steam has to apply before it will run an app.
struct AppUpdate {
    std::uint64_t downloaded{0};
    std::uint64_t toDownload{0};
    std::uint64_t staged{0};
    std::uint64_t toStage{0};

    /// How far through it Steam is, 0 to 1: downloading and staging count byte for byte. 0
    /// while Steam has not sized it yet.
    [[nodiscard]] double progress() const noexcept;
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

    /// The update an app's manifest says Steam must apply first; nothing when none is pending
    /// or the app has no manifest.
    [[nodiscard]] std::optional<AppUpdate> pendingUpdate(std::string_view appId) const;

    /// Asks Steam to install an app the next time it starts, into the library folder with the
    /// most free space: writes a manifest marking the app as needing an update. Returns the
    /// manifest. Throws std::runtime_error when there is no folder or the manifest cannot be
    /// written; does nothing to an app that already has a manifest.
    std::filesystem::path requestInstall(std::string_view appId, std::string_view name) const;

  private:
    std::vector<std::filesystem::path> roots_;
};

/// A Provider over a discovered installation. Throws when no installation was
/// found, which the catalog reports as a problem rather than a failure.
class Provider final : public library::Provider {
  public:
    explicit Provider(Library library);

    [[nodiscard]] Source source() const override {
        return Source::Steam;
    }
    [[nodiscard]] std::vector<Game> list() override;

  private:
    Library library_;
};

} // namespace iideck::library::steam