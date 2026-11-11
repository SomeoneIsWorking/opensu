// core_system_files — the data a built-in core reads from the runner's system folder besides BIOS
// files (Dolphin's `Sys`): one folder of a zip the catalog pins by URL and SHA-256 (the core's own
// source at the revision it is built from), unpacked file by file under `<system>/<into>`. Its
// record `<records>/<core>.system.json` names the pin and every file openSU wrote; only those are
// ever replaced or removed, so a file the player put there (a BIOS in Dolphin's `Sys/GC`) stays.
#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_catalog.hpp"
#include "net/web_client.hpp"

namespace opensu::library::roms {

/// What a record says was installed.
struct InstalledSystemFiles {
    SystemFilesSource source;
    /// Relative to `<system>/<source.into>`, generic form.
    std::vector<std::string> files;
};

class CoreSystemFiles {
  public:
    /// Files go under `system`; records beside the cores in `records`.
    CoreSystemFiles(std::filesystem::path system, std::filesystem::path records)
        : system_{std::move(system)}, records_{std::move(records)} {
    }

    /// The record of `core`'s files, or nothing when openSU installed none.
    [[nodiscard]] std::optional<InstalledSystemFiles> installed(std::string_view core) const;
    /// Whether `core`'s files are the ones `source` pins.
    [[nodiscard]] bool current(std::string_view core, const SystemFilesSource& source) const;

    /// Downloads `source`, checks its SHA-256 and writes its folder's files for `core`, each one
    /// whole; a file openSU did not write is left as it is, and a file of the previous pin the new
    /// one lacks is removed. The number of files written. A failed download changes nothing; a
    /// write that fails part way is finished by the next install.
    [[nodiscard]] net::Result<std::size_t> install(const std::stop_token& stop,
                                                   const net::WebClient& web, std::string_view core,
                                                   const SystemFilesSource& source,
                                                   const net::WebClient::Progress& progress) const;

    /// Removes the files openSU wrote for `core` and its record. False with `error` when one
    /// stays.
    [[nodiscard]] bool remove(std::string_view core, std::string& error) const;

  private:
    [[nodiscard]] std::filesystem::path record(std::string_view core) const;
    [[nodiscard]] bool writeRecord(std::string_view core, const InstalledSystemFiles& files,
                                   std::string& error) const;

    std::filesystem::path system_;
    std::filesystem::path records_;
};

} // namespace opensu::library::roms
