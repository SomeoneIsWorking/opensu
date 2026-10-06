// The Gamescope wrapper: exact argv, with and without a refresh rate.
#include "launch/gamescope.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using iideck::launch::Output;
using iideck::launch::wrapInGamescope;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

using Args = std::vector<std::string>;

} // namespace

int main() {
    iideck::library::LaunchSpec inner{.program = "steam", .args = {"-silent", "-applaunch", "440"}};

    const auto withRefresh = wrapInGamescope(Output{2560, 1440, 144}, inner);
    expect(withRefresh.program == "gamescope", "the program is gamescope");
    expect(withRefresh.args == Args{"-W", "2560", "-H", "1440", "-w", "2560", "-h", "1440", "-r",
                                    "144", "-f", "--", "steam", "-silent", "-applaunch", "440"},
           "output size, refresh and inner command are passed through");

    const auto noRefresh = wrapInGamescope(Output{1920, 1080, 0}, inner);
    expect(noRefresh.args == Args{"-W", "1920", "-H", "1080", "-w", "1920", "-h", "1080", "-f",
                                  "--", "steam", "-silent", "-applaunch", "440"},
           "-r is omitted without a refresh rate");

    const auto bare = wrapInGamescope(Output{1280, 720, -1},
                                      iideck::library::LaunchSpec{.program = "game", .args = {}});
    expect(bare.args ==
               Args{"-W", "1280", "-H", "720", "-w", "1280", "-h", "720", "-f", "--", "game"},
           "a command without arguments ends at the program");

    std::printf("gamescope: all checks passed\n");
    return 0;
}
