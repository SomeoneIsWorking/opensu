// A test's own directory under the build's test scratch: empty when made, removed afterwards, and
// named with the process id, so no run sees what another run left.
#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#include <unistd.h>

namespace opensu::test {

class ScratchDir {
  public:
    /// `<base>/<name>-<pid>`.
    ScratchDir(const std::filesystem::path& base, const std::string& name)
        : path_{base / (name + "-" + std::to_string(getpid()))} {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScratchDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;
    ScratchDir(ScratchDir&&) = delete;
    ScratchDir& operator=(ScratchDir&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

} // namespace opensu::test
