#include "launch/desktop_windows.hpp"
#include "shell_session.hpp"

namespace opensu::app {
namespace {

/// The desktop as it is (Windows, macOS): the game's windows are the desktop's, nothing is hidden
/// and there is no focus to follow. The shell's window is raised over the game and lowered behind
/// it (`game_overlay_raised_window.cpp`).
class DesktopSession final : public ShellSession {
  public:
    [[nodiscard]] launch::GameWindows* gameWindows() noexcept override {
        return &windows_;
    }
    void hideSteamWindows(bool /*hides*/) override {
    }
    [[nodiscard]] std::unique_ptr<display::FocusSource> focusSource() override {
        return nullptr;
    }
    [[nodiscard]] bool coversMonitor() const noexcept override {
        return false;
    }

  private:
    launch::DesktopWindows windows_;
};

} // namespace

std::unique_ptr<ShellSession> openShellSession(const config::Config& /*config*/) {
    return std::make_unique<DesktopSession>();
}

SessionHelp sessionHelp() noexcept {
    return {
        .run = "  opensu                run the shell in a window on the desktop; Steam and games\n"
               "                        open their own, and the shell is raised over them\n",
        .hidden =
            "  opensu --hidden       the shell with a hidden window, no pads and a free control\n"
            "                        port (logged), for maintainer runs\n",
        .sound = "        --sound         with --hidden, play the UI sounds on the default audio\n"
                 "                        device\n",
        .options = {},
        .environment = {},
    };
}

std::optional<int> runInOwnSession(const config::Config& /*config*/,
                                   const config::Arguments& /*arguments*/,
                                   const std::vector<std::string>& /*args*/) {
    return std::nullopt;
}

} // namespace opensu::app
