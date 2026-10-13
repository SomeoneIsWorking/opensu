// emulators — which emulator starts each system's games on this machine, and how.
//
// The emulators are the catalog's (`emulator_catalog`). One is installed when openSU downloaded
// its AppImage, or, for each of its launchers in turn, a program of it is on PATH, an AppImage of
// it is in one of the usual folders, or its Flatpak is installed; the first found is the one that
// runs. A built-in emulator is installed when its core is found (`libretro_cores`) and runs on
// openSU's runner, `opensu-retro`. The first emulator of the catalog found for a system serves it,
// and an override from configuration replaces the search for its system.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "appimage_runtime.hpp"
#include "config/config.hpp"
#include "downloaded_emulators.hpp"
#include "emulator_catalog.hpp"
#include "game.hpp"
#include "libretro_cores.hpp"

namespace opensu::library::roms {

/// Where emulators are looked for.
struct EmulatorSearch {
    std::vector<std::filesystem::path> executablePath;
    std::vector<std::filesystem::path> appImageDirs;
    /// Flatpak installations' `app` directories.
    std::vector<std::filesystem::path> flatpakDirs;
    /// openSU's own downloads; empty for none.
    std::filesystem::path downloads;
    /// Whether AppImages can start here.
    FuseSupport fuse;
    /// Where built-in emulators' cores are looked for.
    CoreSearch cores;
    /// openSU's libretro runner, which built-in emulators start; empty for none.
    std::filesystem::path runner;
    /// Where the runner keeps save RAM, one folder per system; empty leaves it the runner's.
    std::filesystem::path saves;

    /// PATH, then openSU's downloads under `dataDir`, ~/Applications, ~/AppImages and
    /// ~/.local/bin for AppImages, then the user's and the system's Flatpak installations; cores
    /// as `CoreSearch::standard` looks for them, run by `runner` with save RAM under `saves`.
    [[nodiscard]] static EmulatorSearch standard(const std::filesystem::path& home,
                                                 const std::filesystem::path& dataDir,
                                                 std::vector<std::filesystem::path> executablePath,
                                                 std::filesystem::path runner,
                                                 std::filesystem::path saves);
};

/// How an emulator got onto this machine.
enum class InstallSource : std::uint8_t {
    /// A program on PATH: a system package or the player's own build.
    Program,
    /// An AppImage the player put in one of the usual folders.
    AppImage,
    /// An AppImage openSU downloaded.
    Downloaded,
    Flatpak,
    /// A core openSU downloaded from the libretro buildbot.
    DownloadedCore,
    /// A core in RetroArch's or the system's core folder.
    Core,
};

/// The source's name as the player reads it.
[[nodiscard]] std::string_view label(InstallSource source);
/// Whether openSU downloaded it, so it updates and removes it.
[[nodiscard]] bool downloadedByOpensu(InstallSource source) noexcept;

/// An emulator found here.
struct EmulatorInstall {
    std::string id;
    InstallSource source{InstallSource::Program};
    /// The program, the AppImage, the Flatpak's app id, or the core.
    std::string where;
    /// Empty when the source does not say.
    std::string version;
    /// The program and its words, `{rom}` where the game goes.
    std::vector<std::string> command;
    /// Why it cannot start here, with what fixes it; empty when it can.
    std::string problem;
};

class Emulators {
  public:
    /// Each system's command: words with `{rom}` where the game's file goes, or the file
    /// appended when no word holds it.
    using Commands = std::map<std::string, std::vector<std::string>, std::less<>>;

    /// Finds the catalog's emulators. `overrides` are keyed by system ("ps2").
    [[nodiscard]] static Emulators discover(EmulatorCatalog catalog, const EmulatorSearch& search,
                                            const Commands& overrides);

    [[nodiscard]] const EmulatorCatalog& catalog() const noexcept {
        return catalog_;
    }

    /// The emulator `id` as found here, or nothing when it is not installed.
    [[nodiscard]] const EmulatorInstall* installed(std::string_view id) const;

    /// Every emulator of `system`, each as it would start `rom`: installed or not, in the order
    /// the catalog lists them. A configured override is listed first as "Custom".
    [[nodiscard]] std::vector<EmulatorOption> options(std::string_view system,
                                                      const std::filesystem::path& rom) const;

    /// The name of the emulator `launch` uses for `system`, or nothing when none serves it.
    [[nodiscard]] std::string chosenName(std::string_view system) const;

    /// The command that starts `rom` on `system`; nothing when no emulator for it was found.
    [[nodiscard]] std::optional<LaunchSpec> launch(std::string_view system,
                                                   const std::filesystem::path& rom) const;

    /// The emulators that would serve `system`, for telling the player what to install.
    [[nodiscard]] std::vector<std::string_view> candidates(std::string_view system) const;

    /// The name an override from configuration is listed under.
    static constexpr std::string_view customName = "Custom";

  private:
    EmulatorCatalog catalog_;
    Commands overrides_;
    /// The installed emulators by id.
    std::map<std::string, EmulatorInstall, std::less<>> installs_;
    Commands commands_;
    /// The emulator found for each system, by name.
    std::map<std::string, std::string, std::less<>> names_;
};

/// What a search for emulators needs: the catalog, where to look, the configured overrides.
struct EmulatorSetup {
    EmulatorCatalog catalog;
    EmulatorSearch search;
    Emulators::Commands overrides;
    /// The buildbot folder cores are downloaded from, ending in /.
    std::string coreSource{buildbotNightly};

    /// The standard search for `config`, with the catalog shipped in its assets folder.
    [[nodiscard]] static EmulatorSetup standard(const config::Config& config);

    /// Searches now; what is installed changes as the player installs and removes.
    [[nodiscard]] Emulators discover() const {
        return Emulators::discover(catalog, search, overrides);
    }
};

} // namespace opensu::library::roms
