// A launch rule's writes through `writeLaunchSettings`: what it leaves in a file, and that applying
// it again over what it wrote writes nothing.
#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "library/emulator_launch_config.hpp"

namespace opensu::test {

/// What `file` says after `setup`'s rule wrote it, having said `player` before, or been missing.
inline std::string writtenOver(const library::LaunchSetup& setup, const std::filesystem::path& file,
                               std::optional<std::string_view> player) {
    if (player) {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream{file, std::ios::binary} << *player;
    }
    library::writeLaunchSettings(setup);
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// Every file under `setup`'s settings and data folders, its time pushed a day back first; true
/// when `writeLaunchSettings` then leaves each file and time as it was and adds none.
inline bool rewriteWritesNothing(const library::LaunchSetup& setup) {
    namespace fs = std::filesystem;
    const auto stamped = [&setup] {
        std::map<fs::path, fs::file_time_type> times;
        for (const fs::path& root : {setup.config, setup.data}) {
            std::error_code missing;
            if (root.empty() || !fs::is_directory(root, missing)) {
                continue;
            }
            for (const auto& entry : fs::recursive_directory_iterator{root}) {
                if (entry.is_regular_file()) {
                    times.emplace(entry.path(), fs::last_write_time(entry.path()));
                }
            }
        }
        return times;
    };
    for (const auto& [file, time] : stamped()) {
        fs::last_write_time(file, time - std::chrono::hours{24});
    }
    const std::map<fs::path, fs::file_time_type> before = stamped();
    library::writeLaunchSettings(setup);
    return stamped() == before;
}

} // namespace opensu::test
