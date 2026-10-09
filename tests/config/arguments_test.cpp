// The command line as typed values.
#include "config/arguments.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

} // namespace

int main() {
    using opensu::config::Arguments;
    const Arguments none = Arguments::parse({});
    expect(!none.hidden && !none.renderPath && !none.keyboardPrompts && !none.help,
           "no arguments is the plain shell");
    const std::vector<std::string> hidden{"--hidden"};
    expect(Arguments::parse(hidden).hidden, "--hidden asks for the hidden run");
    expect(!Arguments::parse(hidden).startsSteam() && Arguments{}.startsSteam(),
           "a hidden run never starts Steam");
    const std::vector<std::string> render{"--render", "a.png", "--keyboard"};
    const Arguments rendered = Arguments::parse(render);
    expect(rendered.renderPath == "a.png" && rendered.keyboardPrompts && !rendered.hidden,
           "--render takes its file");
    const std::vector<std::string> bare{"--render"};
    expect(!Arguments::parse(bare).renderPath, "--render without a file is ignored");
    std::printf("arguments: all checks passed\n");
    return 0;
}
