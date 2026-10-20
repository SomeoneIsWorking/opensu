// switch_info — what a Switch game file says about itself: the title in its Control NCA's
// control.nacp and its icon, read offline with the player's own keys (Eden's prod.keys and
// title.keys), and kept in the artwork cache by the file's size and modification time.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "dump_info.hpp"
#include "switch_keys.hpp"

namespace opensu::library::roms {

class SwitchInfo {
  public:
    /// Reads nothing: no keys and no cache.
    SwitchInfo() = default;

    /// Keys come from the first of `keyDirs` that holds prod.keys; titles and icons are kept in
    /// `cacheDir`.
    SwitchInfo(std::vector<std::filesystem::path> keyDirs, std::filesystem::path cacheDir);

    /// Where Eden keeps its keys: its data folder, then its Flatpak's.
    [[nodiscard]] static std::vector<std::filesystem::path>
    edenKeyDirs(const std::filesystem::path& xdgDataHome, const std::filesystem::path& home);

    /// Starts a listing: the keys are read again when first needed and the counts start over.
    void begin();

    /// The title and icon `file` (an NSP or XCI) carries, from the cache when the file is
    /// unchanged. Nothing, with the file and the step that failed logged, when they cannot be read.
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
