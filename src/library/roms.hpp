// roms — the games in ROM folders: one folder per system under a ROM root, each holding game
// files or game folders, started by the emulator found for the system.
#pragma once

#include <filesystem>
#include <vector>

#include "arcade_names.hpp"
#include "emulators.hpp"
#include "game.hpp"

namespace opensu::library::roms {

/// The ROM roots on this machine: a folder named ROM, ROMs or roms (or Emulation/roms) in the
/// home folder or at the top of a mounted drive under `mountDirs`, holding at least one
/// system's folder.
[[nodiscard]] std::vector<std::filesystem::path>
discoverRoots(const std::filesystem::path& home,
              const std::vector<std::filesystem::path>& mountDirs);

/// The folders drives are mounted under for the user whose home is `home`.
[[nodiscard]] std::vector<std::filesystem::path>
standardMountDirs(const std::filesystem::path& home);

class Provider final : public library::Provider {
  public:
    Provider(std::vector<std::filesystem::path> roots, Emulators emulators, NameDb names);

    [[nodiscard]] Source source() const override {
        return Source::Rom;
    }

    /// Every game in every known system's folder under the roots, titled from its name with the
    /// dump tags cleaned off, and an arcade game from the kept listing when it has its short name.
    /// A game whose system has no emulator is listed, with `unavailable` naming what to install.
    [[nodiscard]] std::vector<Game> list() override;

    [[nodiscard]] const std::vector<std::filesystem::path>& roots() const noexcept {
        return roots_;
    }

  private:
    std::vector<std::filesystem::path> roots_;
    Emulators emulators_;
    NameDb names_;
};

} // namespace opensu::library::roms
