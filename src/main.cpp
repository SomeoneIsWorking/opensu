// Command opensu is a gamepad-first shell for a game library. It shows Steam,
// Epic, GOG and emulator ROMs in one grid and launches each title into the
// runtime that already owns it.
//
// It is not an emulator and ships no games: Steam and Legendary keep doing their
// own authentication, downloading and cloud sync.
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "app/shell_app.hpp"
#include "config/config.hpp"
#include "lucent/log.h"
#include "session/monitor.hpp"
#include "session/nested_session.hpp"

namespace {

void printHelp() {
    std::printf("opensu — a gamepad-first game library shell\n"
                "\n"
                "  opensu                run the shell; from a desktop it starts its own\n"
                "                        Gamescope, and Steam and games run inside it\n"
                "  opensu --render FILE  render one frame to FILE and exit\n"
                "        --keyboard      with --render, draw the keyboard's prompts\n"
                "\n"
                "Environment:\n"
                "  OPENSU_STEAM_ROOTS  colon-separated Steam install roots\n"
                "  OPENSU_ROM_ROOTS    colon-separated ROM roots (discovered when unset)\n"
                "  OPENSU_EMULATORS    system=program arg {rom};system2=program\n"
                "  OPENSU_WIDTH        window width (default 1280)\n"
                "  OPENSU_HEIGHT       window height (default 800)\n"
                "  OPENSU_HOME_MODE    home grid: standard (scrolling, default) or wiisu\n"
                "                      (pages with peeks and page dots)\n"
                "  OPENSU_ASSETS       directory holding the typeface (default ../share/opensu\n"
                "                      beside the executable)\n"
                "  OPENSU_CONTROL_PORT control channel port (default 7311)\n"
                "  OPENSU_SESSION      name of the session's scopes (default opensu-<pid>)\n"
                "\n"
                "Control channel, on loopback only:\n"
                "  GET  /state       the shell's state as JSON\n"
                "  POST /input       a button name: up down left right a b x y l1 r1\n"
                "                    select start guide\n"
                "  GET  /frame.png   the next frame, as PNG bytes\n"
                "  POST /quit        close the shell\n");
}

} // namespace

int main(int argc, char** argv) {
    // Lucent reads its own debug channels from the environment, so there is
    // nothing to initialise here. Everything else is read once, by config::read.
    const opensu::config::Config& config = opensu::config::read();

    const std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<std::string> renderPath;
    bool keyboardPrompts = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--render" && i + 1 < args.size()) {
            renderPath = args[++i];
        } else if (args[i] == "--keyboard") {
            keyboardPrompts = true;
        } else if (args[i] == "--help" || args[i] == "-h") {
            printHelp();
            return 0;
        }
    }

    // Without a Gamescope of its own, opensu makes one and runs inside it, so that
    // Steam and every game share the one compositor that closing opensu ends.
    if (!renderPath && !config.insideGamescope && !config.sessionInherited) {
        const std::optional<opensu::session::Output> output = opensu::session::readMonitor();
        if (!output) {
            lucent::error("session", "no display to read the monitor from");
            return 1;
        }
        opensu::session::NestedSession session{config.session, config.gamescope};
        return session.run(*output, args);
    }

    opensu::app::Settings settings{
        .width = config.width,
        .height = config.height,
        .controlPort = config.controlPort,
        .controlChannel = config.controlChannel,
        .homeMode = config.homeMode,
    };
    opensu::app::ShellApp shell{settings};

    // Rendering one frame to a file needs no window, which is how the layout can
    // be looked at without a compositor.
    if (renderPath) {
        return shell.renderToFile(*renderPath, keyboardPrompts) ? 0 : 1;
    }
    return shell.run();
}
