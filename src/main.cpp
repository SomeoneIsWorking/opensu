// Command iideck is a gamepad-first shell for a game library. It shows Steam,
// Epic, GOG and emulator ROMs in one grid and launches each title into the
// runtime that already owns it.
//
// It is not an emulator and ships no games: Steam, Legendary and Heroic keep
// doing their own authentication, downloading and cloud sync.
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
    std::printf("iideck — a gamepad-first game library shell\n"
                "\n"
                "  iideck                run the shell; from a desktop it starts its own\n"
                "                        Gamescope, and Steam and games run inside it\n"
                "  iideck --render FILE  render one frame to FILE and exit\n"
                "\n"
                "Environment:\n"
                "  IIDECK_STEAM_ROOTS  colon-separated Steam install roots\n"
                "  IIDECK_ROM_ROOTS    colon-separated ROM directories\n"
                "  IIDECK_EMULATORS    SYSTEM=program arg;SYSTEM2=program\n"
                "  IIDECK_WIDTH        window width (default 1280)\n"
                "  IIDECK_HEIGHT       window height (default 800)\n"
                "  IIDECK_HOME_MODE    home grid: standard (scrolling, default) or wiisu\n"
                "                      (pages with peeks and page dots)\n"
                "  IIDECK_ASSETS       directory holding the typeface\n"
                "  IIDECK_GAMEPAD      only accept controllers whose name contains this\n"
                "  IIDECK_CONTROL_PORT control channel port (default 7311)\n"
                "  IIDECK_SESSION      name of the session's scopes (default iideck-<pid>)\n"
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
    const iideck::config::Config& config = iideck::config::read();

    const std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<std::string> renderPath;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--render" && i + 1 < args.size()) {
            renderPath = args[++i];
        } else if (args[i] == "--help" || args[i] == "-h") {
            printHelp();
            return 0;
        }
    }

    // Without a Gamescope of its own, iideck makes one and runs inside it, so that
    // Steam and every game share the one compositor that closing iideck ends.
    if (!renderPath && !config.insideGamescope && !config.sessionInherited) {
        const std::optional<iideck::session::Output> output = iideck::session::readMonitor();
        if (!output) {
            lucent::error("session", "no display to read the monitor from");
            return 1;
        }
        iideck::session::NestedSession session{config.session, config.executablePath};
        return session.run(*output, args);
    }

    iideck::app::Settings settings{
        .width = config.width,
        .height = config.height,
        .controlPort = config.controlPort,
        .controlChannel = config.controlChannel,
        .homeMode = config.homeMode,
    };
    iideck::app::ShellApp shell{settings};

    // Rendering one frame to a file needs no window, which is how the layout can
    // be looked at without a compositor.
    if (renderPath) {
        return shell.renderToFile(*renderPath) ? 0 : 1;
    }
    return shell.run();
}
