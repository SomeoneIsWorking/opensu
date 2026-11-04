// legendary's `list-saves` read as the newest cloud save, and a save's manifest name as its time;
// output as legendary's cli.py prints it.

#include <cstdio>
#include <cstdlib>

#include "save_sync/epic_cloud.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::save_sync::cloudSaveTime;
using opensu::save_sync::newestCloudSave;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void cloudSavesRead() {
    expect(newestCloudSave("[cli] INFO: Getting list of saves...\n"
                           "Save games:\n"
                           "- Quest (\"Quest\")\n"
                           " + 2026.10.30-09.15.00.manifest\n"
                           " + 2026.11.04-18.09.00.manifest\r\n"
                           " + 2026.11.01-23.59.59.manifest\n") == "2026.11.04-18.09.00.manifest",
           "the newest of three, one on CRLF");
    expect(newestCloudSave("Save games:\n").empty(), "no cloud save");
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
