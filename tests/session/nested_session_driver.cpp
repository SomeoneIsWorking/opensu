// The nested session with a stand-in for the shell, for tools/nested_remap's restart check: the
// real CompositorSession runs the real Gamescope, whose child is this program again, which then
// only waits. `nested_session_driver <session> <gamescope> <desktop note> <width> <height>`.
#include <string>
#include <vector>

#include <unistd.h>

#include "config/config.hpp"
#include "host/desktop_request.hpp"
#include "lucent/log.h"
#include "session/compositor_session.hpp"
#include "support/test_main.hpp"

int main(int argc, char** argv) try {
    if (opensu::config::read().insideGamescope) {
        lucent::info("driver", "child up in gamescope (pid {})", getpid());
        pause();
        return 0;
    }
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() != 5) {
        lucent::error("driver", "usage: <session> <gamescope> <desktop note> <width> <height>");
        return 2;
    }
    opensu::session::CompositorSession session{args[0], args[1], opensu::config::read().stateDir,
                                               opensu::host::DesktopRequest{args[2]}};
    const opensu::display::Output output{std::stoi(args[3]), std::stoi(args[4]), 60};
    return session.run(output, {});
} catch (...) {
    return opensu::test::escaped();
}
