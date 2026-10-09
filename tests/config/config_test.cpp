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
    const opensu::config::Config& config = opensu::config::read();
    const std::string which{argv[1]};
    using opensu::config::timeFormatIs24Hour;
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
        using opensu::config::gamescopeBeside;
        const std::filesystem::path rel{OPENSU_GAMESCOPE_RELATIVE};
        expect(gamescopeBeside("/prefix/bin/opensu") == "/prefix" / rel,
               "an installed opensu finds the fork under its prefix");
        expect(gamescopeBeside("/build/src/opensu") == "/build" / rel,
               "a build-tree opensu finds the staged fork under the build directory");
        expect(gamescopeBeside("").empty(), "an unknown executable has no fork");
        expect(config.gamescope.filename() == "gamescope", "the config names the fork binary");
        std::printf("config: all checks passed\n");
        return 0;
    }
    if (which == "data-dir") {
        expect(config.dataDir == "/xdg-data/opensu", "XDG_DATA_HOME holds opensu's data");
        std::printf("config: all checks passed\n");
        return 0;
    }
    if (which == "data-dir-default") {
        expect(config.dataDir == config.home / ".local" / "share" / "opensu",
               "opensu's data defaults to ~/.local/share/opensu");
        std::printf("config: all checks passed\n");
        return 0;
    }
    if (which == "config-dir") {
        expect(config.configDir == "/xdg-config/opensu", "XDG_CONFIG_HOME holds opensu's settings");
        std::printf("config: all checks passed\n");
        return 0;
    }
    if (which == "config-dir-default") {
        expect(config.configDir == config.home / ".config" / "opensu",
               "opensu's settings default to ~/.config/opensu");
        std::printf("config: all checks passed\n");
        return 0;
    }
    using opensu::config::HomeMode;
    if (which == "home-default" || which == "home-invalid") {
        expect(config.homeMode == HomeMode::Standard, "an unset or unknown home mode is Standard");
    } else if (which == "home-standard") {
        expect(config.homeMode == HomeMode::Standard, "standard selects Standard");
    } else if (which == "home-wiisu") {
        expect(config.homeMode == HomeMode::WiiSu, "wiisu selects WiiSu");
    } else if (which == "default") {
        expect(config.session == "opensu-" + std::to_string(getpid()),
               "the default session is opensu-<pid>");
        expect(!config.sessionInherited, "a default session is not inherited");
    } else {
        expect(config.session == "opensu-test-named", "OPENSU_SESSION names the session");
        expect(config.sessionInherited, "a session from the environment is inherited");
    }
    std::printf("config: all checks passed\n");
    return 0;
}
