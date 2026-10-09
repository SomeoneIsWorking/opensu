// The downloaders' install logs: legendary 0.20.35 (cli.py, downloader/mp/manager.py) and gogdl
// 1.3.1 (dl/progressbar.py), both on '[%(name)s] %(levelname)s: %(message)s'.
#include "library/install_log.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

using opensu::library::install_log::loggedError;
using opensu::library::install_log::progress;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void testProgress() {
    expect(progress("[DLManager] INFO: = Progress: 12.34% (505/4096), Running for "
                    "00:00:10, ETA: 00:01:11") == 0.1234,
           "legendary's progress line is its percentage");
    expect(progress("[DLManager] INFO: = Progress: 100.00% (4096/4096), Running for "
                    "00:01:20, ETA: 00:00:00") == 1.0,
           "the last legendary line is one");
    expect(progress("[PROGRESS] INFO: = Progress: 12.34 505/4096, Running for: 00:00:10, "
                    "ETA: 00:01:11") == 0.1234,
           "gogdl's progress line has no percent sign");
    expect(progress("[PROGRESS] INFO: = Progress: 0.00 0/4096, Running for: 00:00:00, "
                    "ETA: 00:00:00") == 0.0,
           "gogdl's first line is zero");
    expect(!progress("[DLManager] INFO:  - Downloaded: 104.20 MiB, Written: 250.10 MiB"),
           "the downloaded line is not progress");
    expect(!progress("[PROGRESS] INFO: = Downloaded: 1.00 MiB, Written: 1.00 MiB"),
           "gogdl's downloaded line is not progress");
    expect(!progress("[cli] INFO: Download size: 1024.50 MiB (Compression savings: 50.0%)"),
           "a percentage elsewhere is not progress");
    expect(!progress("[DLManager] INFO: = Progress: soon%"), "a damaged line is not progress");
    expect(!progress("[PROGRESS] INFO: = Progress: 12.34"), "a number alone is not progress");
    expect(!progress(""), "an empty line is not progress");
}

void testLoggedError() {
    expect(loggedError("[cli] ERROR: Login failed! Cannot continue with download process.") ==
               "Login failed! Cannot continue with download process.",
           "an error line is the reason");
    expect(loggedError("[MAIN] ERROR: No builds found \r") == "No builds found",
           "a trailing carriage return and spaces are dropped");
    expect(loggedError("[cli] CRITICAL: Installation cannot proceed, exiting.") ==
               "Installation cannot proceed, exiting.",
           "a critical line is the reason");
    expect(!loggedError("[cli] INFO: Install size: 2048.00 MiB"), "an info line is no failure");
    expect(!loggedError("The ERROR: word alone"), "text mentioning an error is no failure");
}

} // namespace

int main() {
    testProgress();
    testLoggedError();
    std::printf("install_log: all checks passed\n");
    return 0;
}
