// Command iideck is a gamepad-first shell for a game library. It shows Steam,
// Epic, GOG and emulator ROMs in one grid and launches each title into the
// runtime that already owns it.
//
// It is not an emulator and ships no games: Steam, Legendary and Heroic keep
// doing their own authentication, downloading and cloud sync.
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "app/shell_app.hpp"
#include "lucent/log.h"

namespace {

int envInt(const char* name, int fallback)
{
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
        return fallback;
    }
    try {
        return std::stoi(raw);
    } catch (const std::exception&) {
        return fallback;
    }
}

bool envBool(const char* name, bool fallback)
{
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
        return fallback;
    }
    const std::string text{raw};
    return text == "1" || text == "true" || text == "yes";
}

std::vector<std::string> splitList(const char* raw)
{
    std::vector<std::string> out;
    if (raw == nullptr) {
        return out;
    }
    std::string current;
    for (const char* p = raw;; ++p) {
        if (*p == ':' || *p == '\0') {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
            if (*p == '\0') {
                break;
            }
            continue;
        }
        current.push_back(*p);
    }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    // Lucent reads its own debug channels from the environment, so there is
    // nothing to initialise here.

    iideck::app::Settings settings{
        .width = envInt("IIDECK_WIDTH", 1280),
        .height = envInt("IIDECK_HEIGHT", 800),
        .steamRoots = splitList(std::getenv("IIDECK_STEAM_ROOTS")),
        .romRoots = splitList(std::getenv("IIDECK_ROM_ROOTS")),
        .emulatorCommands = std::getenv("IIDECK_EMULATORS") != nullptr ? std::getenv("IIDECK_EMULATORS") : "",
    };

    iideck::app::ShellApp shell{settings};

    // Rendering one frame to a file needs no window, which is how the layout can
    // be looked at without a compositor.
    for (int i = 1; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "--render" && i + 1 < argc) {
            return shell.renderToFile(argv[++i]) ? 0 : 1;
        }
        if (arg == "--help" || arg == "-h") {
            std::printf("iideck — a gamepad-first game library shell\n"
                        "\n"
                        "  iideck              run the shell\n"
                        "  iideck --render FILE  render one frame to FILE and exit\n"
                        "\n"
                        "Environment:\n"
                        "  IIDECK_STEAM_ROOTS  colon-separated Steam install roots\n"
                        "  IIDECK_ROM_ROOTS    colon-separated ROM directories\n"
                        "  IIDECK_EMULATORS    SYSTEM=program arg;SYSTEM2=program\n"
                        "  IIDECK_WIDTH        window width (default 1280)\n"
                        "  IIDECK_HEIGHT       window height (default 800)\n");
            return 0;
        }
    }

    return shell.run();
}