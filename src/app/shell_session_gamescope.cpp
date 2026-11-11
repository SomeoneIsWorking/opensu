#include "shell_session.hpp"

#include "host/desktop_request.hpp"
#include "host/login_session_end.hpp"
#include "host/runner.hpp"
#include "host/session_mode.hpp"
#include "lucent/log.h"
#include "session/compositor_session.hpp"
#include "session/gamescope_windows.hpp"
#include "session/login_session.hpp"
#include "session/monitor.hpp"
#include "session/steam_window_hiding.hpp"

namespace opensu::app {
namespace {

class GamescopeShellSession final : public ShellSession {
  public:
    launch::GameWindows* gameWindows() noexcept override {
        return &windows_;
    }
    void hideSteamWindows(bool hides) override {
        steamWindowHiding_.hide(hides);
    }
    std::unique_ptr<display::FocusSource> focusSource() override {
        return std::make_unique<session::GamescopeWindows>();
    }
    bool coversMonitor() const noexcept override {
        return true;
    }

  private:
    session::GamescopeWindows windows_;
    session::SteamWindowHiding steamWindowHiding_;
};

} // namespace

std::unique_ptr<ShellSession> openShellSession(const config::Config& config) {
    if (!config.insideGamescope) {
        return nullptr;
    }
    return std::make_unique<GamescopeShellSession>();
}

SessionHelp sessionHelp() noexcept {
    return {
        .run = "  opensu                run the shell; from a desktop it starts its own\n"
               "                        Gamescope, and Steam and games run inside it\n",
        .hidden =
            "  opensu --hidden       the shell with an unmapped window, no Gamescope, no pads\n"
            "                        and a free control port (logged), for maintainer runs\n",
        .sound = "        --sound         with --hidden, play the UI sounds on the default audio\n"
                 "                        device (PIPEWIRE_NODE can move them to another sink)\n",
        .options =
            "  opensu --session      openSU is the login session, started by the display manager\n"
            "                        on a top-level Gamescope; the power list offers Switch to\n"
            "                        desktop instead of Quit to desktop\n",
        .environment =
            "  OPENSU_SESSION      name of the session's scopes (default opensu-<pid>)\n",
    };
}

std::optional<int> runInOwnSession(const config::Config& config, const config::Arguments& arguments,
                                   const std::vector<std::string>& args) {
    // Without a Gamescope of its own, opensu makes one and runs inside it, so that Steam and every
    // game share the one compositor that closing opensu ends.
    session::CompositorSession session{config.session, config.gamescope, config.stateDir,
                                       host::DesktopRequest{config.desktopRequest}};
    if (arguments.loginSession) {
        // No display yet: Gamescope takes the seat at the native mode.
        session::LoginSession login{
            std::move(session),
            host::LoginSessionEnd{host::systemRunner(), host::selectorPath,
                                  host::DesktopRequest{config.desktopRequest}},
            config.executablePath};
        return login.run(args);
    }
    std::optional<display::Output> output = session::readMonitor();
    if (!output) {
        lucent::error("session", "no display to read the monitor from");
        return 1;
    }
    output->scale = config.displayScale.value_or(output->scale);
    return session.run(*output, args);
}

} // namespace opensu::app
