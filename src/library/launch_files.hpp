// launch_files — files a standalone emulator reads, set for one launch openSU starts and put back
// after. The player's file is kept beside it as `<file>.opensu-player`, or an empty
// `<file>.opensu-absent` marks a file the player did not have; both are undone byte for byte when
// the launch ends, or by `recover` at the next launch after a crash.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::library {

/// What one file says while the launch runs.
struct RunFile {
    std::filesystem::path path;
    std::string bytes;
};

class LaunchFiles {
  public:
    /// The marker the player's file is kept under while the emulator runs.
    static constexpr std::string_view keptSuffix{".opensu-player"};
    /// The marker of a file the launch made, which the player did not have.
    static constexpr std::string_view absentSuffix{".opensu-absent"};

    /// Puts back each of `files` a crashed launch left set.
    static void recover(const std::vector<std::filesystem::path>& files);

    /// Sets each of `files` for the run. One that cannot be set stays the player's and is logged.
    explicit LaunchFiles(const std::vector<RunFile>& files);
    LaunchFiles(const LaunchFiles&) = delete;
    LaunchFiles& operator=(const LaunchFiles&) = delete;
    LaunchFiles(LaunchFiles&&) = delete;
    LaunchFiles& operator=(LaunchFiles&&) = delete;
    /// Puts the player's files back.
    ~LaunchFiles();

  private:
    std::vector<std::filesystem::path> set_;
};

} // namespace opensu::library
