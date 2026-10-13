// Command opensu-retro runs one game in a libretro core, in its own window: the built-in emulator
// openSU launches like any other.
//
//   opensu-retro --core <path.so> [--saves <dir>] [--system <dir>] [--hidden] [--validate]
//                [--frames N] [--frame-png <file>] <rom>
//
// Exit status: 0 on quit, 1 for a bad command line or a window, Vulkan or frame-file failure, 2
// when the core or game cannot be loaded, 3 when --validate saw Vulkan validation errors.
// OPENSU_RETRO_CONTROL names the file the control channel's port, or a load failure, is left in.
#include <charconv>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "config/config.hpp"
#include "lucent/log.h"
#include "retro/runner_app.hpp"

namespace {

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
        } else if (arg == "--validate") {
            options.validation = true;
        } else if (arg == "--core" || arg == "--saves" || arg == "--system" ||
                   arg == "--frame-png" || arg == "--frames") {
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
            } else if (arg == "--frame-png") {
                options.framePng = *given;
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
        lucent::error("retro", "usage: opensu-retro --core <path.so> [--saves <dir>] "
                               "[--system <dir>] [--hidden] [--validate] [--frames N] "
                               "[--frame-png <file>] <rom>");
        return std::nullopt;
    }
    if (options.framePng && !options.hidden) {
        lucent::error("retro", "--frame-png needs --hidden");
        return std::nullopt;
    }
    options.rom = *rom;
    return options;
}

/// A core's name without the buildbot's suffix: "mgba" for mgba_libretro.so.
std::string coreName(const std::filesystem::path& core) {
    std::string name = core.stem().string();
    constexpr std::string_view suffix = "_libretro";
    if (name.ends_with(suffix)) {
        name.erase(name.size() - suffix.size());
    }
    return name;
}

} // namespace

int main(int argc, char** argv) {
    const opensu::config::Config config = opensu::config::read();
    const std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<opensu::retro::RunnerOptions> options = parse(args);
    if (!options) {
        return 1;
    }
    if (options->saves.empty()) {
        options->saves = config.savesDir / coreName(options->core);
    }
    if (options->system.empty()) {
        options->system = config.retroSystemDir;
    }
    options->width = config.width;
    options->height = config.height;
    options->fullscreen = config.insideGamescope && !options->hidden;
    options->control = config.retroControl;
    opensu::retro::RunnerApp app{std::move(*options)};
    return static_cast<int>(app.run());
}
