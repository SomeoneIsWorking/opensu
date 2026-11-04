// legendary's `-v sync-saves` with both transfers skipped read as the cloud save it compares with,
// and a save's manifest name as its time; output as legendary's cli.py and core.py log it.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "save_sync/epic_cloud.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::save_sync::cloudSaveIn;
using opensu::save_sync::cloudSaveTime;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void cloudSavesRead() {
    const std::string got = "[cli] INFO: Got 1 remote save game(s)\n"
                            "[cli] INFO: Checking \"Quest\" (Quest)\n";
    expect(cloudSaveIn(got + "[Core] DEBUG: Local save date: 2026-11-04 18:09:20.512+00:00, "
                             "Remote save date: 2026-11-04 18:09:00+00:00\r\n"
                             "[cli] INFO: Save game for \"Quest\" is up to date, skipping...\n",
                       "Quest") == "2026.11.04-18.09.00.manifest",
           "both sides saved: the debug line's cloud time, on CRLF");
    expect(cloudSaveIn(got + "[cli] INFO: Cloud save for \"Quest\" is newer:\n"
                             "[cli] INFO: - Cloud save date: 2026-11-04 18:09:00\n"
                             "[cli] INFO: - Local save date: N/A\n"
                             "[cli] INFO: Save game downloading is disabled, skipping...\n",
                       "Quest") == "2026.11.04-18.09.00.manifest",
           "only the cloud saved: its date line");
    expect(cloudSaveIn("[cli] INFO: Got 0 remote save game(s)\n"
                       "[cli] INFO: Checking \"Quest\" (Quest)\n"
                       "[cli] INFO: Local save for \"Quest\" is newer\n"
                       "[cli] INFO: - Cloud save date: N/A\n",
                       "Quest") == "",
           "no cloud save");
    expect(!cloudSaveIn("[cli] INFO: Got 0 remote save game(s)\n", "Quest"),
           "a game legendary did not check");
    expect(!cloudSaveIn(got, "Que"), "another game's check");
    expect(cloudSaveTime("2026.11.04-18.09.00.manifest") == 1793815740, "a manifest's UTC time");
    expect(!cloudSaveTime("2026.11.04-18.09.manifest"), "a name with no time");
    expect(!cloudSaveTime(""), "no name");
}

} // namespace

int main() try {
    cloudSavesRead();
    std::printf("epic_cloud: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
