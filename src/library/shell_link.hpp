// shell_link — a Windows `.lnk` shortcut, read portably from its bytes (MS-SHLLINK): the program it
// starts, the arguments, the folder it starts in and its icon. A `.lnk` is an untrusted file, so
// every field is bounds-checked; one that runs past the file ends the read with what came before.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace opensu::library {

/// What a shortcut holds, its paths as Windows writes them, in UTF-8.
struct ShellLink {
    std::string target;
    std::string arguments;
    std::string workingDirectory;
    /// The file its icon is in; empty for the target's own.
    std::string iconLocation;
    /// Which icon of that file: the n-th, or minus the resource id when negative.
    std::int32_t iconIndex{0};
    /// The shortcut's comment.
    std::string description;

    bool operator==(const ShellLink&) const = default;
};

/// What `bytes` say; nothing when they are not a shell link.
[[nodiscard]] std::optional<ShellLink> parseShellLink(std::span<const std::uint8_t> bytes);

/// What the shortcut file says; nothing when it cannot be read or is not one.
[[nodiscard]] std::optional<ShellLink> readShellLink(const std::filesystem::path& file);

} // namespace opensu::library
