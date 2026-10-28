// What the player is told when a game is over: a crash by its signal, the runner's load failure
// first, the launch's failure, and nothing alarming for a game that exited or was closed.
#include <csignal>
#include <cstdio>

#include "game_end.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

void crashesAreNamedBySignal() {
    expect(app::crashName(128 + SIGSEGV) == "SIGSEGV", "SIGSEGV is named");
    expect(app::crashName(128 + SIGABRT) == "SIGABRT", "SIGABRT is named");
    expect(app::crashName(128 + 40) == "signal 40", "an unnamed signal is numbered");
    for (const int ended : {SIGTERM, SIGKILL, SIGINT, SIGHUP}) {
        expect(app::crashName(128 + ended).empty(), "a signal that ends a game is no crash");
    }
    expect(app::crashName(0).empty() && app::crashName(1).empty() && app::crashName(-1).empty(),
           "an exit or no status is no crash");
}

void noticesSayHowTheGameEnded() {
    const app::EndNotice crashed = app::noticeOf(app::GameEnd{"Metroid", "", 128 + SIGSEGV, ""});
    expect(crashed.text == "Metroid crashed (SIGSEGV)" && crashed.error, "a crash is an error");

    const app::EndNotice failed = app::noticeOf(app::GameEnd{
        .title = "Metroid",
        .failure = "Metroid closed before it showed a window",
        .exitStatus = 2,
        .runnerError = "mGBA cannot load /roms/metroid.gba",
    });
    expect(failed.text == "mGBA cannot load /roms/metroid.gba" && failed.error,
           "the runner's load failure is told instead of the launch's");

    const app::EndNotice refused =
        app::noticeOf(app::GameEnd{"Metroid", "Metroid did not start", -1, ""});
    expect(refused.text == "Metroid did not start" && refused.error, "a launch failure is told");

    const app::EndNotice exited = app::noticeOf(app::GameEnd{"Metroid", "", 0, ""});
    expect(exited.text == "Metroid closed" && !exited.error, "a clean exit is no error");
    const app::EndNotice closed = app::noticeOf(app::GameEnd{"Metroid", "", -1, ""});
    expect(closed.text == "Metroid closed" && !closed.error, "a game the player closed neither");
    const app::EndNotice stopped = app::noticeOf(app::GameEnd{"Metroid", "", 128 + SIGTERM, ""});
    expect(!stopped.error, "a game ended by SIGTERM is no crash");
    const app::EndNotice cancelled = app::noticeOf(app::GameEnd{
        .title = "Metroid", .failure = "", .exitStatus = -1, .runnerError = "", .cancelled = true});
    expect(cancelled.text == "Metroid launch cancelled" && !cancelled.error,
           "a cancelled launch says so, not that the game closed");
}

} // namespace

int main() {
    crashesAreNamedBySignal();
    noticesSayHowTheGameEnded();
    std::printf("game_end: all checks passed\n");
    return 0;
}
