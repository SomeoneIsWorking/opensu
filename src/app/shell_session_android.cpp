#include "shell_session.hpp"

namespace opensu::app {
namespace {

/// The shell's Activity and the runner's, each in a task of its own: the shell comes to the front
/// over a paused game and sends it back (`game_overlay_activity_task.cpp`). Android tells nothing
/// of a game's windows or focus.
class ActivityTaskSession final : public ShellSession {
  public:
    [[nodiscard]] launch::GameWindows* gameWindows() noexcept override {
        return nullptr;
    }
    void hideSteamWindows(bool /*hides*/) override {
    }
    [[nodiscard]] std::unique_ptr<display::FocusSource> focusSource() override {
        return nullptr;
    }
    [[nodiscard]] bool coversMonitor() const noexcept override {
        return false;
    }
};

} // namespace

std::unique_ptr<ShellSession> openShellSession(const config::Config& /*config*/) {
    return std::make_unique<ActivityTaskSession>();
}

std::optional<int> runInOwnSession(const config::Config& /*config*/,
                                   const config::Arguments& /*arguments*/,
                                   const std::vector<std::string>& /*args*/) {
    return std::nullopt;
}

} // namespace opensu::app
