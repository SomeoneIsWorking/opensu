// switch_info — what a Switch game file says about itself: the title in its Control NCA's
// control.nacp, its icon, its title id and its saves' sizes, read offline with the player's own
// keys (Eden's prod.keys and title.keys), and kept in the artwork cache by the file's size and
// modification time.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "dump_info.hpp"
#include "emulator_catalog.hpp"
#include "switch_keys.hpp"

namespace opensu::library::roms {

class SwitchInfo {
  public:
    /// Reads nothing: no keys and no cache.
    SwitchInfo() = default;

    /// Keys come from the first of `keyDirs` that holds prod.keys; titles and icons are kept in
    /// `cacheDir`.
    SwitchInfo(std::vector<std::filesystem::path> keyDirs, std::filesystem::path cacheDir);

    /// Where yuzu's line keeps its keys, `keys` beside each NAND of `catalog` (its Flatpak's
    /// after its own), in catalog order.
    [[nodiscard]] static std::vector<std::filesystem::path>
    keyDirsOf(const EmulatorCatalog& catalog);

    /// Starts a listing: the keys are read again when first needed and the counts start over.
    void begin();

    /// The title, icon, title id and save sizes `file` (an NSP or XCI) carries, from the cache
    /// when the file is unchanged. Nothing, with the file and the step that failed logged, when
    /// they cannot be read.
    [[nodiscard]] std::optional<DumpInfo> read(const std::filesystem::path& file);

    /// Logs how many of the listing's Switch games were titled from their own files.
    void finish() const;

  private:
    [[nodiscard]] const SwitchKeys* keys();
    [[nodiscard]] std::optional<DumpInfo> extract(const std::filesystem::path& file,
                                                  const std::filesystem::path& stem,
                                                  std::string& error);

    std::vector<std::filesystem::path> keyDirs_;
    std::filesystem::path cacheDir_;
    std::optional<SwitchKeys> keys_;
    bool keysTried_{false};
    std::string keysError_;
    int fromCache_{0};
    int extracted_{0};
    int failed_{0};
    int unkeyed_{0};
};

} // namespace opensu::library::roms
