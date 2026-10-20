// roms — the games in ROM folders: one folder per system under a ROM root, each holding game
// files or game folders, started by the emulator found for the system.
#pragma once

#include <filesystem>
#include <vector>

#include "arcade_names.hpp"
#include "emulators.hpp"
#include "game.hpp"
#include "net/web_client.hpp"
#include "romset_match.hpp"

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
    /// Arcade romsets are matched with listings kept under `cacheDir`; an empty one matches none.
    Provider(std::vector<std::filesystem::path> roots, EmulatorSetup emulators, NameDb names,
             const std::filesystem::path& cacheDir);

    [[nodiscard]] Source source() const override {
        return Source::Rom;
    }

    /// Every game in every known system's folder under the roots, titled from its name with the
    /// dump tags cleaned off, and an arcade game from the kept listing when it has its short name.
    /// A game whose system has no emulator is listed, with `unavailable` naming what to install.
    /// An arcade zip's emulator is the one its romset matches (`applyRomsets`).
    /// The emulators are searched for on every call, so one installed since shows.
    [[nodiscard]] std::vector<Game> list() override;

    [[nodiscard]] const std::vector<std::filesystem::path>& roots() const noexcept {
        return roots_;
    }

  private:
    std::vector<std::filesystem::path> roots_;
    EmulatorSetup emulators_;
    NameDb names_;
    net::WebClient web_;
    RomsetMatcher romsets_;
};

} // namespace opensu::library::roms
