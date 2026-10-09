// desktop_request — the note a login session leaves for itself that it is going to the desktop.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace opensu::host {

/// A file in the runtime directory. Switch to desktop writes it and ends Gamescope; the login
/// session finds it once Gamescope is gone and starts the desktop in its place, so the display
/// manager's autologin never starts openSU again.
class DesktopRequest {
  public:
    explicit DesktopRequest(std::filesystem::path file);

    /// Writes the note; empty when it did, else why not.
    [[nodiscard]] std::string write() const;

    /// Whether the note is there. It is removed either way.
    [[nodiscard]] bool consume() const;

    /// Runs `startplasma-wayland` (found in `path`) in this process's place. Returns only when
    /// it could not be started, with the reason.
    [[nodiscard]] static std::string enterDesktop(const std::vector<std::filesystem::path>& path);

  private:
    std::filesystem::path file_;
};

/// The desktop the session hands over to.
inline constexpr const char* desktopProgram = "startplasma-wayland";

} // namespace opensu::host
