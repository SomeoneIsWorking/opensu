// resume_file — the resume state Save and close leaves for a game, apart from the player's state
// slot: a line naming the format, a JSON line saying which core wrote it, when, and the save RAM it
// goes with, then the core's serialized state. The runner writes and loads it; the shell reads its
// header for the details page and removes it for Start fresh.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace opensu::retro {

struct ResumeHeader {
    /// The core's name as `coreName` gives it ("mgba"), which decides whether a core may load it.
    std::string core;
    /// What the core calls itself ("mGBA") and its version, for the player.
    std::string coreLabel;
    std::string coreVersion;
    std::chrono::system_clock::time_point saved;
    /// CRC-32 of the save RAM when the state was taken.
    std::uint32_t saveRam{0};
};

struct ResumeContents {
    ResumeHeader header;
    std::vector<std::byte> state;
};

/// Writes `state` with `header` to `file`, replacing it only once the write is whole.
[[nodiscard]] bool writeResume(const std::filesystem::path& file, const ResumeHeader& header,
                               std::span<const std::byte> state, std::string& error);

/// The header and state in `file`; nothing with `error` empty when there is no file, else with
/// why it cannot be read.
[[nodiscard]] std::optional<ResumeContents> readResume(const std::filesystem::path& file,
                                                       std::string& error);

/// The header alone; nothing when there is no file or it is not a resume state.
[[nodiscard]] std::optional<ResumeHeader> readResumeHeader(const std::filesystem::path& file);

/// Removes `file`; true when it is gone, also when it was never there.
[[nodiscard]] bool discardResume(const std::filesystem::path& file, std::string& error);

} // namespace opensu::retro
