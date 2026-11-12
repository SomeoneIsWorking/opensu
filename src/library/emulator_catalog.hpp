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
    /// The AppImage inside the asset when the asset is a zip holding one, as an ECMAScript
    /// regular expression over its name; empty when the asset is the AppImage.
    std::string member{};
    /// The asset's SHA-256 the catalog pins (a core release's `"sha256"`), lower-case hex; empty
    /// to take the one the release publishes.
    std::string sha256{};
    /// The SHA-256 of the core library inside a core release's asset (`"library"`), lower-case
    /// hex; empty when the catalog pins none.
    std::string library{};
};

/// A core release's tag and the SHA-256 of the library inside every build's asset (`"library"`).
struct PinnedLibraries {
    std::string tag;
    std::vector<std::string> sha256;
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

/// The data a built-in core reads from the system folder besides BIOS files (`"systemFiles"`):
/// one folder of a zip pinned by URL and SHA-256, the source tree at the revision the core is built
/// from, unpacked under the system folder (`core_system_files`).
struct SystemFilesSource {
    /// An HTTPS zip; every redirect hop stays on its origin.
    std::string url;
    /// Lower-case hex.
    std::string sha256;
    /// The folder inside the zip whose files are taken: "dolphin-<revision>/Data/Sys".
    std::string folder;
    /// Where they go under the system folder: "dolphin-emu/Sys".
    std::string into;

    bool operator==(const SystemFilesSource&) const = default;
};

/// Whether `path` is relative, in generic normal form and stays inside the folder it is taken
/// under: a system files `folder`, `into` or member.
[[nodiscard]] bool isInsidePath(std::string_view path);

struct EmulatorEntry {
    /// Stable key: the download folder's name and the settings' spelling.
    std::string id;
    std::string name;
    std::vector<std::string> systems;
    /// Where the emulator keeps its own settings; empty when not known. For an emulator that
    /// keeps them in `$XDG_CONFIG_HOME/<name>` whenever that is set (`"xdgConfig"`), that folder
    /// when the session sets it, the catalog's `config` otherwise.
    std::filesystem::path config;
    /// Where its Flatpak keeps them; empty for an emulator without one.
    std::filesystem::path flatpakConfig;
    /// Where it keeps its data, when that is apart from its settings; empty otherwise.
    std::filesystem::path data;
    /// Where its Flatpak keeps its data; empty for an emulator without one or without `data`.
    std::filesystem::path flatpakData;
    /// In the order they are looked for. A downloaded AppImage runs with the first one's words.
    /// None for a built-in emulator.
    std::vector<EmulatorLauncher> launchers;
    /// Absent for an emulator without an official AppImage. For a built-in emulator, the release
    /// its core comes from on this build instead of the buildbot (`"release"`'s `"assets"`).
    std::optional<ReleaseSource> release;
    /// A built-in emulator's release tag and every build's library, when the catalog pins them.
    std::optional<PinnedLibraries> libraries;
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
    /// The data its core reads from the system folder (`"systemFiles"`); absent for most cores.
    std::optional<SystemFilesSource> systemFiles;
    /// The systems of `systems` its core links two players on (`"links"`), when it comes from
    /// its release.
    std::vector<std::string> links;

    [[nodiscard]] bool serves(std::string_view system) const;
    [[nodiscard]] bool builtIn() const noexcept {
        return !core.empty();
    }
    /// Where the install it runs as keeps its settings: its Flatpak's folder when `flatpak` and it
    /// has one, else `config`.
    [[nodiscard]] const std::filesystem::path& configOf(bool flatpak) const noexcept {
        return flatpak && !flatpakConfig.empty() ? flatpakConfig : config;
    }
    /// Where the install it runs as keeps its data, as `configOf` its settings.
    [[nodiscard]] const std::filesystem::path& dataOf(bool flatpak) const noexcept {
        return flatpak && !flatpakData.empty() ? flatpakData : data;
    }
    /// Its name without the catalog's " (built-in)" mark, where the context says it is the core:
    /// "mGBA" for "mGBA (built-in)".
    [[nodiscard]] std::string_view shortName() const noexcept;
    /// Whether a built-in emulator's core downloaded from `url` came from its release.
    [[nodiscard]] bool fromRelease(std::string_view url) const noexcept {
        return release && url.starts_with(release->downloads);
    }
    /// Whether openSU can download it: an official AppImage, or a core from the buildbot or its
    /// release.
    [[nodiscard]] bool downloadable() const noexcept {
        return release.has_value() || builtIn();
    }
};

/// The core build another player compares: `release:<id>@<tag>` when `librarySha256` is a library
/// `entry`'s release pins for any build, which share that tag's source, else
/// `sha256:<librarySha256>`, also for a core with no catalog entry.
[[nodiscard]] std::string coreBuildOf(const EmulatorEntry* entry, std::string_view librarySha256);

class EmulatorCatalog {
  public:
    EmulatorCatalog() = default;
    explicit EmulatorCatalog(std::vector<EmulatorEntry> entries) : entries_{std::move(entries)} {
    }

    /// The catalog in `json` for the core build `build` (which of a core release's assets it
    /// takes), with `~` in a config path meaning `home` and `xdgConfigHome` the session's
    /// `$XDG_CONFIG_HOME`, empty when unset. Nothing, with `error` naming the entry and field,
    /// when it is malformed.
    [[nodiscard]] static std::optional<EmulatorCatalog>
    parse(std::string_view json, const std::filesystem::path& home, std::string& error,
          std::string_view build = coreTarget.build,
          const std::filesystem::path& xdgConfigHome = {});

    /// The catalog shipped in `assetsDir`; empty, with the reason logged, when it cannot be read.
    [[nodiscard]] static EmulatorCatalog shipped(const std::filesystem::path& assetsDir,
                                                 const std::filesystem::path& home,
                                                 const std::filesystem::path& xdgConfigHome = {});

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
