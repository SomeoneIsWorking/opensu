// emulators — which emulator starts each system's games on this machine, and how.
//
// An emulator is found as a program on PATH, an AppImage in one of the usual folders, or an
// installed Flatpak, in that order; the first emulator of a system's list that is found serves
// it. An override from configuration replaces the search for its system.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace opensu::library::roms {

/// Where emulators are looked for.
struct EmulatorSearch {
    std::vector<std::filesystem::path> executablePath;
    std::vector<std::filesystem::path> appImageDirs;
    /// Flatpak installations' `app` directories.
    std::vector<std::filesystem::path> flatpakDirs;

    /// PATH, then ~/Applications, ~/AppImages and ~/.local/bin for AppImages, then the user's
    /// and the system's Flatpak installations.
    [[nodiscard]] static EmulatorSearch standard(const std::filesystem::path& home,
                                                 std::vector<std::filesystem::path> executablePath);
};

class Emulators {
  public:
    /// Each system's command: words with `{rom}` where the game's file goes, or the file
    /// appended when no word holds it.
    using Commands = std::map<std::string, std::vector<std::string>, std::less<>>;

    /// Finds an emulator for every system it can. `overrides` are keyed by system ("ps2").
    [[nodiscard]] static Emulators discover(const EmulatorSearch& search,
                                            const Commands& overrides);

    /// The command that starts `rom` on `system`; nothing when no emulator for it was found.
    [[nodiscard]] std::optional<LaunchSpec> launch(std::string_view system,
                                                   const std::filesystem::path& rom) const;

    /// The emulators that would serve `system`, for telling the player what to install.
    [[nodiscard]] static std::vector<std::string_view> candidates(std::string_view system);

  private:
    Commands commands_;
};

} // namespace opensu::library::roms
