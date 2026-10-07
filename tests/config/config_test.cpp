// Values read from the environment. ctest sets or unsets the variable per test;
// the argument says which to expect.
#include "config/config.hpp"

#include <cstdio>
#include <cstdlib>
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
