// save_memory — the core's save RAM and its .srm file: loaded before the first frame, written
// whole (a .part file renamed over the old one) when the RAM differs from what was last written.
#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace opensu::retro {

class SaveMemory {
  public:
    explicit SaveMemory(std::filesystem::path file);

    /// Fills `ram` from the file when it is exactly that size; otherwise `ram` stays blank and the
    /// file stays as it is until the core changes the RAM.
    void load(std::span<std::byte> ram);

    /// Writes `ram` when it changed since the load or the last write, and logs the write. False,
    /// logged, when the write fails; the next call tries again.
    bool flush(std::span<const std::byte> ram);

    [[nodiscard]] const std::filesystem::path& file() const {
        return file_;
    }

  private:
    std::filesystem::path file_;
    std::vector<std::byte> written_;
};

} // namespace opensu::retro
