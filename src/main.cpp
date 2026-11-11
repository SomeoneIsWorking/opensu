// Command opensu is a gamepad-first shell for a game library. It shows Steam,
// Epic, GOG and emulator ROMs in one grid and launches each title into the
// runtime that already owns it.
//
// It is not an emulator and ships no games: Steam and Legendary keep doing their
// own authentication, downloading and cloud sync.
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
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
    const opensu::app::SessionHelp session = opensu::app::sessionHelp();
    const char separator = opensu::config::pathListSeparator;
    std::printf(
        "opensu — a gamepad-first game library shell\n"
        "\n"
        "%.*s"
        "  opensu --render FILE  render one frame to FILE and exit\n"
        "        --keyboard      with --render, draw the keyboard's prompts\n"
        "%.*s"
        "        --steam         with --hidden, start the Steam client in its mode; end the\n"
        "                        run with POST /quit so Steam shuts down with it\n"
        "%.*s"
        "%.*s"
        "\n"
        "Environment:\n"
        "  OPENSU_STEAM_ROOTS  '%c'-separated Steam install roots\n"
        "  OPENSU_ROM_ROOTS    '%c'-separated ROM roots (discovered when unset)\n"
        "  OPENSU_EMULATORS    system=program arg {rom};system2=program\n"
        "  OPENSU_WIDTH        window width in dp, pixels with --hidden (default 1280)\n"
        "  OPENSU_HEIGHT       window height in dp, pixels with --hidden (default 800)\n"
        "  OPENSU_DISPLAY_SCALE  pixels per dp a hidden window is laid out for (default 1)\n"
        "  OPENSU_HOME_MODE    home grid: standard (scrolling, default) or wiisu\n"
        "                      (pages with peeks and page dots)\n"
        "  OPENSU_ASSETS       directory holding the typeface (default ../share/opensu\n"
        "                      beside the executable)\n"
        "  OPENSU_CONTROL_PORT control channel port (default 7311)\n"
        "  OPENSU_LAN_PORT     port other openSUs reach this one on with LAN on\n"
        "                      (default 7312; a hidden run takes a free one)\n"
        "%.*s"
        "\n"
        "Control channel, on loopback only:\n"
        "  GET  /state       the shell's state as JSON\n"
        "  POST /input       a button name: up down left right a b x y l1 r1\n"
        "                    select start guide\n"
        "  GET  /frame.png   the next frame, as PNG bytes\n"
        "  POST /quit        close the shell\n",
        static_cast<int>(session.run.size()), session.run.data(),
        static_cast<int>(session.hidden.size()), session.hidden.data(),
        static_cast<int>(session.sound.size()), session.sound.data(),
        static_cast<int>(session.options.size()), session.options.data(), separator, separator,
        static_cast<int>(session.environment.size()), session.environment.data());
}

} // namespace

int main(int argc, char** argv) try {
    // Lucent reads its own debug channels from the environment, so there is
    // nothing to initialise here. Everything else is read once, by config::read.
    const opensu::config::Config config = opensu::config::read();
    std::error_code stateError;
    std::filesystem::create_directories(config.logFile.parent_path(), stateError);
    lucent::set_log_outputs({.file = config.logFile.string()});
    if (stateError) {
        lucent::warn("openSU", "cannot create {}: {}", config.stateDir.string(),
                     stateError.message());
    }
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
        .scale = {.stated = config.displayScale,
                  .unscaled = config.insideGamescope,
                  .connector = config.insideGamescope},
        .controlPort = arguments.hidden ? std::uint16_t{0} : config.controlPort,
        .lanPort = arguments.hidden ? std::uint16_t{0} : config.lanPort,
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
