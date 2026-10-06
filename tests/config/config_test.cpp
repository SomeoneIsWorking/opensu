// The session name: IIDECK_SESSION when set, otherwise one made from the pid.
// ctest sets or unsets the variable per test; the argument says which to expect.
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
    expect(argc == 2, "one argument: default or inherited");
    const iideck::config::Config& config = iideck::config::read();
    if (std::string{argv[1]} == "default") {
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
