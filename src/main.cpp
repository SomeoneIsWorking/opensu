// Command opensu is a gamepad-first shell for a game library. It shows Steam,
// Epic, GOG and emulator ROMs in one grid and launches each title into the
// runtime that already owns it.
//
// It is not an emulator and ships no games: Steam and Legendary keep doing their
// own authentication, downloading and cloud sync.
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <vector>

// Renames main to SDL_main where the OS enters through SDL (Android's Activity).
#include <SDL3/SDL_main.h>

#include "app/sdl_log.hpp"
#include "app/shell_app.hpp"
#include "app/shell_session.hpp"
#include "config/arguments.hpp"
#include "config/config.hpp"
#include "fileio/instance_lock.hpp"
#include "lucent/log.h"

namespace {

void printHelp() {
    std::printf(
        "opensu — a gamepad-first game library shell\n"
        "\n"
        "  opensu                run the shell; from a desktop it starts its own\n"
        "                        Gamescope, and Steam and games run inside it\n"
        "  opensu --render FILE  render one frame to FILE and exit\n"
        "        --keyboard      with --render, draw the keyboard's prompts\n"
        "  opensu --hidden       the shell with an unmapped window, no Gamescope, no pads\n"
        "                        and a free control port (logged), for maintainer runs\n"
        "        --steam         with --hidden, start the Steam client in its mode; end the\n"
        "                        run with POST /quit so Steam shuts down with it\n"
        "        --sound         with --hidden, play the UI sounds on the default audio\n"
        "                        device (PIPEWIRE_NODE can move them to another sink)\n"
        "  opensu --session      openSU is the login session, started by the display manager\n"
        "                        on a top-level Gamescope; the power list offers Switch to\n"
        "                        desktop instead of Quit to desktop\n"
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

int main(int argc, char** argv) try {
    // Lucent reads its own debug channels from the environment, so there is
    // nothing to initialise here. Everything else is read once, by config::read.
    const opensu::config::Config config = opensu::config::read();
    const opensu::app::SdlLog sdlLog;

    const std::vector<std::string> args(argv + 1, argv + argc);
    const opensu::config::Arguments arguments = opensu::config::Arguments::parse(args);
    if (arguments.help) {
        printHelp();
        return 0;
    }
    const std::optional<std::string>& renderPath = arguments.renderPath;

    // One openSU at a time: a second would run a second shell and a second Steam client.
    const bool interactive =
        !renderPath && !arguments.hidden && !config.insideGamescope && !config.sessionInherited;
    std::optional<opensu::fileio::InstanceLock> lock;
    if (interactive) {
        lock.emplace(config.runtimeDir / "instance.lock");
        if (!lock->held()) {
            lucent::info("openSU", "another openSU is running; leaving it to it");
            return 0;
        }
    }

    if (interactive) {
        if (const std::optional<int> status =
                opensu::app::runInOwnSession(config, arguments, args)) {
            return *status;
        }
    }

    opensu::app::Settings settings{
        .width = config.width,
        .height = config.height,
        .controlPort = arguments.hidden ? std::uint16_t{0} : config.controlPort,
        .controlChannel = config.controlChannel,
        .homeMode = config.homeMode,
        .hidden = arguments.hidden,
        .loginSession = arguments.loginSession,
        .startSteam = arguments.startsSteam(),
        .playSounds = arguments.playsSounds(),
    };
    opensu::app::ShellApp shell{settings, config};

    // Rendering one frame to a file needs no window, which is how the layout can
    // be looked at without a compositor.
    if (renderPath) {
        return shell.renderToFile(*renderPath, arguments.keyboardPrompts) ? 0 : 1;
    }
    return shell.run();
} catch (const std::exception& failure) {
    lucent::error("openSU", "stopped by an error: {}", failure.what());
    return 1;
}
