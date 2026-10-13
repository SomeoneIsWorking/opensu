// appimage_runtime — whether an AppImage can start on this machine. The classic AppImage runtime
// loads libfuse.so.2 to mount itself; newer runtimes carry FUSE inside. Both need the kernel's
// FUSE device and a fusermount helper. A missing piece is named with the command that installs it.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::library::roms {

/// What this machine offers AppImages.
struct FuseSupport {
    bool libfuse2{true};
    bool fusermount{true};
    bool device{true};
    /// The distribution's `ID` and `ID_LIKE` from os-release, for the install command.
    std::string distro;

    /// Read from the library folders, PATH, /dev/fuse and /etc/os-release.
    [[nodiscard]] static FuseSupport
    detect(const std::vector<std::filesystem::path>& executablePath);
};

/// Whether the AppImage's runtime (the ELF before its filesystem image) loads libfuse.so.2.
[[nodiscard]] bool needsLibfuse2(const std::filesystem::path& appImage);

/// Why the AppImage cannot start here, with the command that fixes it; empty when it can.
[[nodiscard]] std::string appImageRefusal(const std::filesystem::path& appImage,
                                          const FuseSupport& fuse);

/// The command that installs libfuse.so.2 on `distro` (os-release `ID` and `ID_LIKE` words).
[[nodiscard]] std::string libfuse2Install(std::string_view distro);

} // namespace opensu::library::roms
