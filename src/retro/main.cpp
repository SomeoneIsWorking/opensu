// Command opensu-retro runs one game in a libretro core, in its own window: the built-in emulator
// openSU launches like any other.
//
//   opensu-retro --core <path.so> [--saves <dir>] [--system <dir>] [--settings <file>]
//                [--option <key>=<value>]... [--players N] [--netplay <file>]
//                [--arcade <buttons>:<ways>] [--hidden | --silent] [--embedded] [--touch <dp>]
//                [--validate] [--frames N] [--frame-png <file>] [--frame-pngs <dir>]
//                <rom>
//
// Exit status: 0 on quit, 1 for a bad command line or a window, Vulkan or frame-file failure, 2
// when the core, the game or its save cannot be loaded, 3 when --validate saw Vulkan validation
// errors. OPENSU_RETRO_CONTROL names the file the control channel's port, or a load failure, is
// left in.
#include <charconv>
#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Renames main to SDL_main where the OS enters through SDL (Android's Activity).
#include <SDL3/SDL_main.h>

#include "config/config.hpp"
#include "lucent/log.h"
#include "lucent/text.h"

#include "retro/embedded.hpp"
#include "retro/joypad.hpp"
#include "retro/runner_app.hpp"
#include "retro/save_paths.hpp"
#include "settings/video_settings.hpp"

namespace {

/// The shell's per-launch settings in `file`; nothing (the reason logged) when they cannot be read.
std::optional<opensu::settings::VideoSettings> readSettings(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        lucent::error("retro", "cannot read the settings file {}", file.string());
        return std::nullopt;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::string error;
    std::optional<opensu::settings::VideoSettings> video =
        opensu::settings::parseVideoJson(text, error);
    if (!video) {
        lucent::error("retro", "{}: {}", file.string(), error);
    }
    return video;
}

/// The command line, or nothing (the reason logged) when it is malformed.
std::optional<opensu::retro::RunnerOptions> parse(std::span<const std::string> args) {
    opensu::retro::RunnerOptions options;
    std::optional<std::filesystem::path> rom;
    for (std::size_t at = 0; at < args.size(); ++at) {
        const std::string_view arg = args[at];
        const auto value = [&]() -> std::optional<std::string> {
            if (at + 1 >= args.size()) {
                lucent::error("retro", "{} needs a value", arg);
                return std::nullopt;
            }
            return args[++at];
        };
        if (arg == "--hidden") {
            options.hidden = true;
            options.silent = true;
        } else if (arg == "--silent") {
            options.silent = true;
        } else if (arg == opensu::retro::embeddedArgument) {
            options.embedded = true;
        } else if (arg == "--validate") {
            options.validation = true;
        } else if (arg == "--core" || arg == "--saves" || arg == "--system" ||
                   arg == "--settings" || arg == "--frame-png" || arg == "--frame-pngs" ||
                   arg == "--frames" || arg == "--option" || arg == "--players" ||
                   arg == "--netplay" || arg == "--arcade" || arg == "--touch") {
            const std::optional<std::string> given = value();
            if (!given) {
                return std::nullopt;
            }
            if (arg == "--core") {
                options.core = *given;
            } else if (arg == "--saves") {
                options.saves = *given;
            } else if (arg == "--system") {
                options.system = *given;
            } else if (arg == "--settings") {
                const std::optional<opensu::settings::VideoSettings> video = readSettings(*given);
                if (!video) {
                    return std::nullopt;
                }
                options.video = *video;
            } else if (arg == "--option") {
                const std::size_t equals = given->find('=');
                if (equals == std::string::npos || equals == 0) {
                    lucent::error("retro", "--option takes key=value, not {}", *given);
                    return std::nullopt;
                }
                options.coreOptions.insert_or_assign(given->substr(0, equals),
                                                     given->substr(equals + 1));
            } else if (arg == "--frame-png") {
                options.framePng = *given;
            } else if (arg == "--frame-pngs") {
                options.framePngs = *given;
            } else if (arg == "--arcade") {
                options.arcade = opensu::retro::cabinetListingOf(*given);
                if (!options.arcade) {
                    lucent::error("retro", "--arcade takes <buttons>:<ways>, not {}", *given);
                    return std::nullopt;
                }
            } else if (arg == "--touch") {
                float dp = 0.0F;
                const auto [end, error] =
                    lucent::text::from_chars_real(given->data(), given->data() + given->size(), dp);
                if (error != std::errc{} || end != given->data() + given->size() || dp <= 0.0F) {
                    lucent::error("retro", "--touch takes pixels per dp, not {}", *given);
                    return std::nullopt;
                }
                options.touchDp = dp;
            } else if (arg == "--netplay") {
                std::ifstream in{*given, std::ios::binary};
                const std::string text{std::istreambuf_iterator<char>{in},
                                       std::istreambuf_iterator<char>{}};
                std::string error;
                options.netplay = opensu::retro::netplaySetupOf(text, error);
                if (!options.netplay) {
                    lucent::error("retro", "{}: {}", *given, error);
                    return std::nullopt;
                }
            } else if (arg == "--players") {
                unsigned players = 0;
                const auto [end, error] =
                    std::from_chars(given->data(), given->data() + given->size(), players);
                if (error != std::errc{} || end != given->data() + given->size() || players == 0 ||
                    players > opensu::retro::maxPlayers) {
                    lucent::error("retro", "--players takes 1 to {}, not {}",
                                  opensu::retro::maxPlayers, *given);
                    return std::nullopt;
                }
                options.players = players;
            } else {
                std::uint64_t frames = 0;
                const auto [end, error] =
                    std::from_chars(given->data(), given->data() + given->size(), frames);
                if (error != std::errc{} || end != given->data() + given->size()) {
                    lucent::error("retro", "--frames takes a count, not {}", *given);
                    return std::nullopt;
                }
                options.frames = frames;
            }
        } else if (arg.starts_with("--") || rom) {
            lucent::error("retro", "unexpected argument {}", arg);
            return std::nullopt;
        } else {
            rom = std::filesystem::path{arg};
        }
    }
    if (options.core.empty() || !rom) {
        lucent::error(
            "retro",
            "usage: opensu-retro --core <path.so> [--saves <dir>] "
            "[--system <dir>] [--settings <file>] [--option <key>=<value>]... "
            "[--players N] [--netplay <file>] [--arcade <buttons>:<ways>] [--hidden | --silent] "
            "[--touch <dp>] [--validate] [--frames N] [--frame-png <file>] "
            "[--frame-pngs <dir>] <rom>");
        return std::nullopt;
    }
    if ((options.framePng || options.framePngs) && !options.hidden) {
        lucent::error("retro", "--frame-png and --frame-pngs need --hidden");
        return std::nullopt;
    }
    if (options.touchDp && !options.hidden) {
        lucent::error("retro", "--touch needs --hidden");
        return std::nullopt;
    }
    options.rom = *rom;
    return options;
}

} // namespace

int main(int argc, char** argv) try {
    const opensu::config::Config config = opensu::config::read();
    const std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<opensu::retro::RunnerOptions> options = parse(args);
    if (!options) {
        return 1;
    }
    if (options->saves.empty()) {
        options->saves = opensu::retro::coreSaveFolder(config.savesDir, options->core);
    }
    if (options->system.empty()) {
        options->system = config.retroSystemDir;
    }
    options->width = config.width;
    options->height = config.height;
    options->fullscreen = config.insideGamescope && !options->hidden;
    options->scale = {.stated = config.displayScale,
                      .unscaled = config.insideGamescope || options->hidden,
                      .connector = options->fullscreen};
    options->control = config.retroControl;
    options->librashader = config.librashader;
    options->touchArt = config.assetsDir / "touch";
    opensu::retro::RunnerApp app{std::move(*options)};
    return static_cast<int>(app.run());
} catch (const std::exception& failure) {
    lucent::error("retro", "stopped by an error: {}", failure.what());
    return 1;
}
