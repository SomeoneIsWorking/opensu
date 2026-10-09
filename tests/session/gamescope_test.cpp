// The Gamescope command line that wraps opensu: exact argv, with and without a refresh rate.
#include "session/gamescope.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using opensu::session::gamescopeArgs;
using opensu::session::Output;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

using Args = std::vector<std::string>;

} // namespace

int main() {
    const auto withRefresh = gamescopeArgs(Output{2560, 1440, 144}, "/usr/bin/opensu", {"--x"});
    expect(withRefresh == Args{"-W", "2560", "-H", "1440", "-w", "2560", "-h", "1440", "-r", "144",
                               "-f", "--close-focused-window", "--", "/usr/bin/opensu", "--x"},
           "output size, refresh and opensu's command are passed through");

    const auto noRefresh = gamescopeArgs(Output{1920, 1080, 0}, "/usr/bin/opensu", {});
    expect(noRefresh == Args{"-W", "1920", "-H", "1080", "-w", "1920", "-h", "1080", "-f",
                             "--close-focused-window", "--", "/usr/bin/opensu"},
           "-r is omitted without a refresh rate, and no arguments end at the program");

    const auto negative = gamescopeArgs(Output{1280, 720, -1}, "opensu", {"a", "b"});
    expect(negative == Args{"-W", "1280", "-H", "720", "-w", "1280", "-h", "720", "-f",
                            "--close-focused-window", "--", "opensu", "a", "b"},
           "a negative refresh rate is omitted");

    std::printf("gamescope: all checks passed\n");
    return 0;
}
