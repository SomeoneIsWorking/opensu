// The login session from Gamescope to the next login, through the real session code with a fake
// gamescope, a fake sudo on PATH and a fake selector: an abnormal end runs the restore and exits
// non-zero, a quiet end does not.
#include "session/login_session.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "host/runner.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using test::expect;

struct Bench {
    explicit Bench(const std::string& gamescopeBody) {
        const std::string name = "opensu-test-login-" + std::to_string(getpid());
        session = name;
        root = fs::path{OPENSU_TEST_SCRATCH} / name;
        fs::remove_all(root);
        fs::create_directories(root / "bin");
        setenv("PATH", (root / "bin").string().append(":/usr/bin:/bin").c_str(), 1);
        setenv("BENCH_DIR", root.c_str(), 1);
        write(root / "bin" / "sudo", "echo \"sudo $*\" >> \"$BENCH_DIR/calls.log\"\n");
        write(root / "bin" / "gamescope", gamescopeBody);
        const std::ofstream selector{root / "selector"};
    }
    ~Bench() {
        fs::remove_all(root);
    }
    Bench(const Bench&) = delete;
    Bench& operator=(const Bench&) = delete;

    static void write(const fs::path& file, const std::string& body) {
        std::ofstream{file} << "#!/bin/sh\n" << body;
        fs::permissions(file, fs::perms::owner_all);
    }
    [[nodiscard]] session::LoginSession login(std::chrono::seconds window) const {
        return session::LoginSession{
            session::CompositorSession{session, root / "bin" / "gamescope"},
            host::LoginSessionEnd{host::systemRunner(), root / "selector",
                                  host::DesktopRequest{root / "run" / "desktop"}, window},
            {root / "bin"}};
    }
    [[nodiscard]] std::string calls() const {
        std::ifstream in{root / "calls.log"};
        return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    }
    [[nodiscard]] std::string restoreCall() const {
        return "sudo -n " + (root / "selector").string() + " restore\n";
    }

    std::string session;
    fs::path root;
};

void aFailingGamescopeHandsTheLoginBack() {
    for (const char* body : {"exit 1\n", "kill -SEGV $$\n", "exit 0\n"}) {
        Bench bench{body};
        session::LoginSession login = bench.login(std::chrono::seconds{3600});
        expect(login.run({"--session"}) != 0, "a failed session exits non-zero");
        expect(bench.calls() == bench.restoreCall(), "the selector restore ran once");
    }
}

void aMissingGamescopeHandsTheLoginBack() {
    Bench bench{"exit 0\n"};
    fs::remove(bench.root / "bin" / "gamescope");
    session::LoginSession login = bench.login(std::chrono::seconds{3600});
    expect(login.run({"--session"}) == 1, "openSU failing before it is up exits 1");
    expect(bench.calls() == bench.restoreCall(), "and restores");
}

void aSessionThatRanAWhileIsLeftAlone() {
    Bench bench{"sleep 2\n"};
    session::LoginSession login = bench.login(std::chrono::seconds{1});
    expect(login.run({"--session"}) == 0, "a clean end after the quick window exits 0");
    expect(bench.calls().empty(), "and changes nothing");
}

} // namespace

int main() {
    fs::create_directories(OPENSU_TEST_SCRATCH);
    aFailingGamescopeHandsTheLoginBack();
    aMissingGamescopeHandsTheLoginBack();
    aSessionThatRanAWhileIsLeftAlone();
    return 0;
}
