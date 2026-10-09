// The login session's note to itself, and the hand-over to the desktop program.
#include "host/desktop_request.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using opensu::host::DesktopRequest;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void noteIsWrittenAndConsumedOnce() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "desktop-request";
    fs::remove_all(root);
    const DesktopRequest request{root / "run" / "desktop"};
    expect(!request.consume(), "there is no note before Switch to desktop");
    expect(request.write().empty() && request.write().empty(),
           "the note is written, twice is fine");
    expect(request.consume(), "the login session finds the note");
    expect(!request.consume(), "and it is gone afterwards, so the desktop is not started twice");
    fs::remove_all(root);
}

void desktopRunsInThePlaceOfTheSession() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "desktop-enter";
    fs::remove_all(root);
    fs::create_directories(root / "bin");
    const fs::path marker = root / "started";
    const fs::path program = root / "bin" / opensu::host::desktopProgram;
    std::ofstream{program} << "#!/bin/sh\necho started > '" << marker.string() << "'\n";
    fs::permissions(program, fs::perms::owner_all);

    expect(!DesktopRequest::enterDesktop({root / "empty"}).empty(),
           "a desktop that is not on PATH is refused, not run");

    const pid_t child = fork();
    expect(child >= 0, "fork");
    if (child == 0) {
        const std::string failure = DesktopRequest::enterDesktop({root / "bin"});
        std::fprintf(stderr, "%s\n", failure.c_str());
        _exit(99);
    }
    int status = 0;
    waitpid(child, &status, 0);
    expect(WIFEXITED(status) && WEXITSTATUS(status) == 0 && fs::exists(marker),
           "the desktop program replaced the process and ran");
    fs::remove_all(root);
}

} // namespace

int main() {
    noteIsWrittenAndConsumedOnce();
    desktopRunsInThePlaceOfTheSession();
    std::printf("desktop_request: all checks passed\n");
    return 0;
}
