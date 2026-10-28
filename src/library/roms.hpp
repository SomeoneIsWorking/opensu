// roms — the games in ROM folders: one folder per system under a ROM root, each holding game
// files or game folders, started by the emulator found for the system.
#pragma once

#include <filesystem>
#include <vector>

#include "arcade_names.hpp"
#include "emulators.hpp"
#include "game.hpp"
#include "rom_tree.hpp"
#include "romset_match.hpp"
#include "switch_info.hpp"

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
    /// Arcade romsets are matched with the listings `romsets` keeps and fetches, which outlives
    /// the provider; null matches none. Switch games are titled by `switchInfo`.
    Provider(std::vector<std::filesystem::path> roots, EmulatorSetup emulators, NameDb names,
             RomsetLists* romsets, SwitchInfo switchInfo);

    [[nodiscard]] Source source() const override {
        return Source::Rom;
    }

    /// Every game in every known system's folder under the roots, titled from its name with the
    /// dump tags cleaned off, and an arcade game from the kept listing when it has its short name.
    /// A PS4 dump or a Switch game file that names itself is titled and shown by what it says.
    /// A game whose system has no emulator is listed, with `unavailable` naming what to install.
    /// An arcade zip's emulator is the one its romset matches (`applyRomsets`); a listing not
    /// kept yet is fetched in the background and never waited on.
    /// The emulators are searched for on every call, so one installed since shows.
    [[nodiscard]] std::vector<Game> list() override;

    [[nodiscard]] const std::vector<std::filesystem::path>& roots() const noexcept {
        return roots_;
    }

  private:
    std::vector<std::filesystem::path> roots_;
    std::unique_ptr<RomTree> tree_{openRomTree()};
    EmulatorSetup emulators_;
    NameDb names_;
    RomsetMatcher romsets_;
    SwitchInfo switch_;
};

} // namespace opensu::library::roms
