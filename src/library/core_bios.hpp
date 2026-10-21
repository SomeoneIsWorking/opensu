// core_bios — a built-in core's BIOS files: the `firmware*` entries of its libretro core info
// (`<core>.info`), what the core does when none is in the runner's system folder, which are there,
// and, for a core that also takes any BIOS image it finds in the folder, the files it will pick by
// its own rule.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "zip/zip_archive.hpp"

namespace opensu::library::roms {

/// The libretro buildbot's archive of every core's info file, on the buildbot's origin.
inline constexpr std::string_view coreInfoArchive = "assets/frontend/info.zip";
inline constexpr std::string_view coreInfoExtension = ".info";

/// What a core does when none of its BIOS files is in the system folder.
enum class WithoutBios : std::uint8_t {
    /// It cannot start the game.
    Needed,
    /// It emulates the BIOS's calls itself (high-level emulation).
    Hle,
    /// It runs an open replacement BIOS built into it, a ROM image of its own.
    Open,
    /// It skips the boot ROM the file would show; the games do not call it.
    Skipped,
};

/// `needed`, `hle`, `open`, `skipped` as the catalog spells them.
[[nodiscard]] std::optional<WithoutBios> withoutBiosNamed(std::string_view spelling);
/// The mode as the player reads it.
[[nodiscard]] std::string_view label(WithoutBios mode) noexcept;

/// One BIOS file a core looks for.
struct CoreFirmware {
    /// Relative to the system folder.
    std::string path;
    /// Without the file name libretro repeats in it.
    std::string description;
    /// The core starts without it.
    bool optional{false};

    bool operator==(const CoreFirmware&) const = default;
};

/// How a core chooses its BIOS from the system folder beyond the names its info lists.
enum class BiosScan : std::uint8_t {
    /// It loads the files its info lists, by name.
    Listed,
    /// PCSX ReARMed's `loadPSXBios`: its own names, then any file of the folder, each kept by
    /// `try_use_bios`'s size and signature check, the first of each region.
    PcsxRearmed,
};

/// `pcsx_rearmed` as the catalog spells it (`"biosScan"`).
[[nodiscard]] std::optional<BiosScan> biosScanNamed(std::string_view spelling);

/// The `firmware*` entries of a core info file, in its order; malformed entries are skipped.
[[nodiscard]] std::vector<CoreFirmware> firmwareIn(std::string_view info);

/// Every core's info file, in the buildbot's `info.zip`.
class CoreInfoArchive {
  public:
    /// Nothing, with `error`, when `zip` is not a zip.
    [[nodiscard]] static std::optional<CoreInfoArchive> open(std::string zip, std::string& error);

    /// `core`'s info file; nothing, with `error`, when the archive does not hold it.
    [[nodiscard]] std::optional<std::string> info(std::string_view core, std::string& error) const;

  private:
    explicit CoreInfoArchive(zip::Archive archive) : archive_{std::move(archive)} {
    }

    zip::Archive archive_;
};

/// A BIOS file PCSX ReARMed takes from the folder, with the region it files it under.
struct PickedBios {
    std::string file;
    /// `US`, `EU` or `JP`, as the core logs it.
    std::string region;

    bool operator==(const PickedBios&) const = default;
};

/// The files PCSX ReARMed's `loadPSXBios` keeps from `system`, in the order it keeps them.
[[nodiscard]] std::vector<PickedBios> pcsxRearmedBiosIn(const std::filesystem::path& system);

/// A core's BIOS as the system folder has it now.
struct CoreBios {
    struct File {
        CoreFirmware firmware;
        bool present{false};
        /// The core boots with it.
        bool used{false};
        /// The region a scanning core files it under; empty otherwise.
        std::string region;
    };
    WithoutBios without{WithoutBios::Needed};
    /// The listed files, then any other file a scanning core takes from the folder.
    std::vector<File> files;

    /// What the core runs on, as the player reads it.
    [[nodiscard]] std::string mode() const;
};

/// `firmware` looked for under `system`, and what a core of `scan` takes from it.
[[nodiscard]] CoreBios biosIn(const std::vector<CoreFirmware>& firmware, WithoutBios without,
                              BiosScan scan, const std::filesystem::path& system);

} // namespace opensu::library::roms
