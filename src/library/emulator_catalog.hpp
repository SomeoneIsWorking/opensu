// emulator_catalog — the emulators openSU knows, read from the shipped `emulators.json`: which
// systems each runs, how it is found and started on this machine, where it keeps its settings,
// where it keeps games' saves, and, for one that publishes an official AppImage, where its
// releases are. An Android
// launcher names the app's activity, started by an intent. A built-in emulator
// names a libretro core instead, which openSU's runner plays.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core_bios.hpp"
#include "core_target.hpp"

namespace opensu::library::roms {

/// One way an emulator is installed and started.
struct EmulatorLauncher {
    /// Program names on PATH.
    std::vector<std::string> programs;
    /// AppImage file names start with this, compared lower-case with only letters and digits.
    std::string appImage;
    /// The Flatpak app id; empty when there is none.
    std::string flatpak;
    /// The Android app's activity, `package/class`; empty when there is none.
    std::string android;
    /// The words after the program, `{rom}` where the game's file goes; for an Android app,
    /// its intent's options (`Launches.applyOptions`).
    std::vector<std::string> args;
};

/// Where an emulator's official releases are published.
struct ReleaseSource {
    /// The latest release as GitHub's or Forgejo's release JSON.
    std::string api;
    /// The asset to download, as an ECMAScript regular expression over its file name.
    std::string asset;
    /// Every download URL must start with this: the project's own release host and path.
    std::string downloads;
};

/// Where an arcade core's ROM-set listings come from: datafiles at fixed HTTPS URLs, merged in
/// order, or the latest release's asset, a zip holding one listing.
struct RomsetSource {
    std::vector<std::string> files;
    std::optional<ReleaseSource> release;

    [[nodiscard]] bool empty() const noexcept {
        return files.empty() && !release;
    }
};

/// How a standalone emulator lays out its games' saves.
enum class SaveLayout : std::uint8_t {
    /// yuzu's line (Eden, Citron, sudachi): `user/save/0000000000000000/<user id>/<title id>`
    /// under its NAND, the profiles in `system/save/8000000000000010/su/avators/profiles.dat`.
    YuzuNand,
    /// Ryujinx's line (Ryubing): save folders under `user/save` of its `bis`, each carrying the
    /// save's attribute in `ExtraData0`, its files in `0` (committed) and `1` (working).
    RyujinxBis,
    /// mGBA: `<rom stem>.sav` beside the ROM, or in the folder its settings name (`mgba_saves`).
    Mgba,
    /// DuckStation: a memory card per game, `<title>_1.mcd`, in its `memcards` folder
    /// (`ps1_cards`).
    DuckStationCards,
};

/// Where a standalone emulator keeps its games' saves (`"saves"`).
struct SaveStore {
    SaveLayout layout{SaveLayout::YuzuNand};
    /// The NAND (yuzu's line), `bis` (Ryujinx's line) or memory card folder; empty for mGBA,
    /// whose settings say.
    std::filesystem::path folder;
    /// The same folder for its Flatpak, under `~/.var/app/<app id>` where the sandbox's XDG
    /// folders put it (`flatpakFolder`); empty for an emulator without one.
    std::filesystem::path flatpakFolder;
};

/// What the catalog appends to a built-in emulator's name.
inline constexpr std::string_view builtInMark = " (built-in)";

/// A core option a built-in emulator's core starts with, as the core spells key and value.
struct CoreOption {
    std::string key;
    std::string value;
};

struct EmulatorEntry {
    /// Stable key: the download folder's name and the settings' spelling.
    std::string id;
    std::string name;
    std::vector<std::string> systems;
    /// Where the emulator keeps its own settings; empty when not known.
    std::filesystem::path config;
    /// Where its Flatpak keeps them; empty for an emulator without one.
    std::filesystem::path flatpakConfig;
    /// In the order they are looked for. A downloaded AppImage runs with the first one's words.
    /// None for a built-in emulator.
    std::vector<EmulatorLauncher> launchers;
    /// Absent for an emulator without an official AppImage.
    std::optional<ReleaseSource> release;
    /// Absent for an emulator whose saves openSU cannot read; a built-in one's are the runner's.
    std::optional<SaveStore> saves;
    /// The libretro core a built-in emulator runs ("mgba_libretro"); empty for a standalone one.
    std::string core;
    /// An arcade core's ROM-set listings; empty for every other emulator.
    RomsetSource romsets;
    /// What a built-in emulator's core does without its BIOS files (`"withoutBios"`).
    WithoutBios withoutBios{WithoutBios::Needed};
    /// How its core chooses a BIOS from the system folder (`"biosScan"`).
    BiosScan biosScan{BiosScan::Listed};
    /// Core options it starts with (`"coreOptions"`): a GPU core's Vulkan renderer.
    std::vector<CoreOption> coreOptions;
    /// Its core ships with openSU (`"shipped"`) rather than coming from the buildbot; it serves a
    /// game only when picked.
    bool shipped{false};

    [[nodiscard]] bool serves(std::string_view system) const;
    [[nodiscard]] bool builtIn() const noexcept {
        return !core.empty();
    }
    /// Its name without the catalog's " (built-in)" mark, where the context says it is the core:
    /// "mGBA" for "mGBA (built-in)".
    [[nodiscard]] std::string_view shortName() const noexcept;
    /// Whether openSU can download it: an official AppImage or a buildbot core.
    [[nodiscard]] bool downloadable() const noexcept {
        return release.has_value() || (builtIn() && !shipped);
    }
};

class EmulatorCatalog {
  public:
    EmulatorCatalog() = default;
    explicit EmulatorCatalog(std::vector<EmulatorEntry> entries) : entries_{std::move(entries)} {
    }

    /// The catalog in `json` for `system` (an entry's `"notOn"` leaves it out there), with `~` in
    /// a config path meaning `home`. Nothing, with `error` naming the entry and field, when it is
    /// malformed.
    [[nodiscard]] static std::optional<EmulatorCatalog>
    parse(std::string_view json, const std::filesystem::path& home, std::string& error,
          std::string_view system = coreTarget.system);

    /// The catalog shipped in `assetsDir`; empty, with the reason logged, when it cannot be read.
    [[nodiscard]] static EmulatorCatalog shipped(const std::filesystem::path& assetsDir,
                                                 const std::filesystem::path& home);

    [[nodiscard]] const std::vector<EmulatorEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] const EmulatorEntry* find(std::string_view id) const;
    [[nodiscard]] const EmulatorEntry* findByName(std::string_view name) const;
    /// The built-in emulators alone, which openSU's runner plays.
    [[nodiscard]] EmulatorCatalog builtInOnly() const;
    /// The keys of the systems an entry serves, in the order the library lists systems.
    [[nodiscard]] std::vector<std::string> servedSystems() const;

  private:
    std::vector<EmulatorEntry> entries_;
};

/// Where the Flatpak `appId` keeps what a host install keeps at `path`: `~/.config` and
/// `~/.local/share` of `home` are the sandbox's `~/.var/app/<app id>/config` and `data`. Empty for
/// a path in neither.
[[nodiscard]] std::filesystem::path flatpakFolder(const std::filesystem::path& path,
                                                  const std::filesystem::path& home,
                                                  std::string_view appId);

/// The shipped catalog's file name in the assets folder.
inline constexpr std::string_view catalogFile = "emulators.json";

} // namespace opensu::library::roms
