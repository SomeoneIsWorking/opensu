// save_memory — the core's save RAM and its .srm file: loaded before the first frame, written
// whole (a .part file renamed over the old one) when the RAM differs from what was last written.
#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <vector>

namespace opensu::retro {

/// A save file the runner cannot put in the core's save RAM; the file is left as it is.
class SaveError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class SaveMemory {
  public:
    explicit SaveMemory(std::filesystem::path file);

    /// Fills the start of `ram` from the file, as RetroArch does; a file smaller than the RAM
    /// leaves the rest as the core made it. Throws `SaveError` when the file is larger than the
    /// RAM or cannot be read, so a save that was not loaded is never written over.
    void load(std::span<std::byte> ram);

    /// Writes `ram` when it differs from what was last loaded or written, and logs the write.
    /// RAM that shrank to an unchanged start of it is unchanged (a core that learned the
    /// cartridge's save type). False, logged, when the write fails; the next call tries again.
    bool flush(std::span<const std::byte> ram);

    [[nodiscard]] const std::filesystem::path& file() const {
        return file_;
    }

  private:
    std::filesystem::path file_;
    std::vector<std::byte> written_;
};

} // namespace opensu::retro
