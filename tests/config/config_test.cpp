// Values read from the environment. ctest sets or unsets the variable per test;
// the argument says which to expect.
#include "config/config.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <unistd.h>

namespace {

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

} // namespace

int main(int argc, char** argv) {
    expect(argc == 2, "one argument naming the case");
    const iideck::config::Config& config = iideck::config::read();
    const std::string which{argv[1]};
    using iideck::config::timeFormatIs24Hour;
    if (which == "time-format") {
        expect(timeFormatIs24Hour("%H:%M:%S"), "the C locale is 24-hour");
        expect(timeFormatIs24Hour("%T"), "%T is 24-hour");
        expect(!timeFormatIs24Hour("%r"), "en_US's %r is 12-hour");
        expect(!timeFormatIs24Hour("%I:%M:%S %p"), "%I is 12-hour");
        expect(!timeFormatIs24Hour("%l:%M %p"), "%l is 12-hour");
        expect(timeFormatIs24Hour("%%I %H"), "an escaped percent is not a directive");
        std::printf("config: all checks passed\n");
        return 0;
    }
    if (which == "gamescope") {
        using iideck::config::gamescopeBeside;
        const std::filesystem::path rel{IIDECK_GAMESCOPE_RELATIVE};
        expect(gamescopeBeside("/prefix/bin/iideck") == "/prefix" / rel,
               "an installed iideck finds the fork under its prefix");
        expect(gamescopeBeside("/build/src/iideck") == "/build" / rel,
               "a build-tree iideck finds the staged fork under the build directory");
        expect(gamescopeBeside("").empty(), "an unknown executable has no fork");
        expect(config.gamescope.filename() == "gamescope", "the config names the fork binary");
        std::printf("config: all checks passed\n");
        return 0;
    }
    if (which == "data-dir") {
        expect(config.dataDir == "/xdg-data/iideck", "XDG_DATA_HOME holds iideck's data");
        std::printf("config: all checks passed\n");
        return 0;
    }
    if (which == "data-dir-default") {
        expect(config.dataDir == config.home / ".local" / "share" / "iideck",
               "iideck's data defaults to ~/.local/share/iideck");
        std::printf("config: all checks passed\n");
        return 0;
    }
    using iideck::config::HomeMode;
    if (which == "home-default" || which == "home-invalid") {
        expect(config.homeMode == HomeMode::Standard, "an unset or unknown home mode is Standard");
    } else if (which == "home-standard") {
        expect(config.homeMode == HomeMode::Standard, "standard selects Standard");
    } else if (which == "home-wiisu") {
        expect(config.homeMode == HomeMode::WiiSu, "wiisu selects WiiSu");
    } else if (which == "default") {
        expect(config.session == "iideck-" + std::to_string(getpid()),
               "the default session is iideck-<pid>");
        expect(!config.sessionInherited, "a default session is not inherited");
    } else {
        expect(config.session == "iideck-test-named", "IIDECK_SESSION names the session");
        expect(config.sessionInherited, "a session from the environment is inherited");
    }
    std::printf("config: all checks passed\n");
    return 0;
}
