// A hidden run's WineRunners reads what is installed and starts nothing: no scheduled check while
// GOG is signed in and GE-Proton is kept current, and Check now and Test are refused.
#include "wine_runners.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include "host_services.hpp"
#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

app::WineRunners::Hooks signedIn() {
    return app::WineRunners::Hooks{.say =
                                       [](const std::string&, bool) {
                                       },
                                   .changed =
                                       [] {
                                       },
                                   .refresh =
                                       [] {
                                       },
                                   .wanted =
                                       [] {
                                           return true;
                                       },
                                   .keepCurrent =
                                       [] {
                                           return true;
                                       },
                                   .inUse =
                                       [] {
                                           return app::WineRunners::InUse{};
                                       },
                                   .gameRunning =
                                       [] {
                                           return false;
                                       }};
}

/// Waits for the start-up measuring to end, so `busy` tells only of a check.
void settle(const app::WineRunners& runners) {
    while (runners.busy()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
}

} // namespace

int main() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "wine_runners"};
    // Nothing listens on the discard port, should a check start after all.
    app::WineRunners hidden{scratch.path(), "http://127.0.0.1:9/release.json", true, signedIn()};
    settle(hidden);
    hidden.service(app::WineRunners::Clock::now());
    expect(!hidden.busy(), "a hidden run starts no check");
    expect(hidden.checkNow() == app::hiddenRunRefusal, "Check now is refused in a hidden run");
    expect(hidden.test("GE-Proton", {"env", "true"}) == app::hiddenRunRefusal,
           "the runner test is refused in a hidden run");
    expect(!hidden.look().checking && hidden.look().lastCheck.empty(), "nothing was checked");

    std::puts("wine_runners: ok");
    return 0;
}
