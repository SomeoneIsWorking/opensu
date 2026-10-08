// steam — reads a local Steam installation: which apps exist, whether they are
// installed, what artwork exists, and when they were last played.
//
// It never talks to the Steam client and never writes.
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

  private:
    [[nodiscard]] std::optional<std::filesystem::path> manifestOf(std::string_view appId) const;
    /// The manifest's StateFlags; 0 when there is none.
    [[nodiscard]] long long stateFlags(std::string_view appId) const;

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