// Install jobs: the store-neutral job's report handling through a fake installer, the Epic job
// against a stub `legendary`, and which stores install.
#include "epic_install_job.hpp"
#include "install_job.hpp"
#include "installs.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using iideck::app::EpicInstallJob;
using iideck::app::InstallJob;
using iideck::app::Installs;
using iideck::library::Source;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// The reports of a job until it finishes, in order, as the loop would take them.
std::vector<InstallJob::Report> drain(InstallJob& job) {
    std::vector<InstallJob::Report> reports;
    const Clock::time_point until = Clock::now() + std::chrono::seconds{20};
    while (Clock::now() < until) {
        if (std::optional<InstallJob::Report> report = job.take()) {
            reports.push_back(*report);
            if (report->finished) {
                return reports;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    expect(false, "the job finished in time");
    return reports;
}

/// An installer that asks for a licence, then finishes as the player answered.
class Asking final : public InstallJob {
  public:
    ~Asking() {
        halt();
    }

  private:
    void run(const std::stop_token& stop, const std::string& appId) override {
        Report asking;
        asking.line = "asking about " + appId;
        asking.licence = true;
        post(asking);
        const std::optional<bool> answer = awaitDecision(stop);
        if (!answer) {
            return;
        }
        Report done;
        done.finished = true;
        if (!*answer) {
            done.failure = "the licence agreement was declined";
        }
        post(done);
    }
};

void testJobLifecycle() {
    Asking job;
    expect(!job.running() && !job.take(), "an idle job has nothing to say");
    expect(job.start("game", "A Game"), "an idle job starts");
    expect(job.running() && job.title() == "A Game", "it runs under its title");
    expect(!job.start("other", "Other"), "a second start is refused while one runs");

    const Clock::time_point until = Clock::now() + std::chrono::seconds{20};
    std::optional<InstallJob::Report> asked;
    while (!asked && Clock::now() < until) {
        asked = job.take();
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    expect(asked && asked->licence && asked->line == "asking about game",
           "a licence question reaches the loop");
    expect(!job.take(), "a report is taken once");
    job.decide(true);
    const std::vector<InstallJob::Report> rest = drain(job);
    expect(rest.back().finished && rest.back().failure.empty(), "an accepted licence finishes");
    expect(!job.running(), "a job is over once its last report is taken");
    expect(job.title() == "A Game", "the title is kept");

    expect(job.start("again", "Again"), "a finished job starts again");
    while (!job.take()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    job.decide(false);
    expect(drain(job).back().failure == "the licence agreement was declined",
           "a declined licence fails the job");
}

void testStoppedJob() {
    {
        Asking job;
        expect(job.start("game", "A Game"), "it starts");
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    expect(true, "a job waiting on the player is stopped and joined when it goes");
}

fs::path writeStub(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream{path} << "#!/bin/sh\n" << body;
    fs::permissions(path, fs::perms::owner_all);
    return path;
}

void testEpic(const fs::path& dir) {
    // Records its arguments, then logs as legendary does: install details, progress, a summary.
    const fs::path ok =
        writeStub(dir / "legendary-ok", R"(echo "$@" > ')" + (dir / "args").string() +
                                            R"('
echo "[cli] INFO: Install size: 2048.00 MiB" >&2
echo "[DLManager] INFO: = Progress: 0.00% (0/4096), Running for 00:00:00, ETA: 00:00:00" >&2
echo "[DLManager] INFO:  - Downloaded: 0.00 MiB, Written: 0.00 MiB" >&2
echo "[DLManager] INFO: = Progress: 12.34% (505/4096), Running for 00:00:10, ETA: 00:01:11" >&2
echo "[DLManager] INFO: = Progress: 100.00% (4096/4096), Running for 00:01:20, ETA: 00:00:00" >&2
exit 0
)");
    EpicInstallJob job{ok.string()};
    expect(job.start("Fortnite", "Fortnite"), "an Epic install starts");
    const std::vector<InstallJob::Report> reports = drain(job);
    expect(reports.back().finished && reports.back().failure.empty(), "it finishes installed");
    std::ifstream args{dir / "args"};
    std::string line;
    std::getline(args, line);
    expect(line == "install Fortnite -y --skip-sdl", "it runs `legendary install <app> -y`");
    // Reports are latest-wins, so only the last of a fast burst is guaranteed to be seen.
    for (const InstallJob::Report& report : reports) {
        if (report.fraction) {
            expect(report.line.find('%') != std::string::npos, "a measured line shows its percent");
            expect(!report.licence, "an Epic install never asks about a licence");
        }
    }

    // A slow stub, so each progress report is taken as it comes.
    const fs::path slow = writeStub(dir / "legendary-slow", R"(sleep 0.3
echo "[DLManager] INFO: = Progress: 12.34% (505/4096), Running for 00:00:10, ETA: 00:01:11" >&2
sleep 0.3
echo "[DLManager] INFO: = Progress: 50.00% (2048/4096), Running for 00:00:20, ETA: 00:00:20" >&2
sleep 0.3
)");
    EpicInstallJob slowJob{slow.string()};
    expect(slowJob.start("Game", "Game"), "a slow Epic install starts");
    std::vector<double> seen;
    for (const InstallJob::Report& report : drain(slowJob)) {
        if (report.fraction) {
            seen.push_back(*report.fraction);
        }
    }
    expect(seen.size() == 2 && seen[0] == 0.1234 && seen[1] == 0.5,
           "progress reaches the loop as legendary logs it");

    const fs::path failing =
        writeStub(dir / "legendary-fails",
                  R"(echo "[cli] ERROR: Login failed! Cannot continue with download process." >&2
echo "[cli] CRITICAL: Installation cannot proceed, exiting." >&2
exit 1
)");
    EpicInstallJob failed{failing.string()};
    expect(failed.start("Game", "Game"), "a failing Epic install starts");
    expect(drain(failed).back().failure == "Login failed! Cannot continue with download process.",
           "it fails with legendary's own reason");

    const fs::path silent = writeStub(dir / "legendary-silent", "exit 4\n");
    EpicInstallJob silentJob{silent.string()};
    expect(silentJob.start("Game", "Game"), "a silent failing install starts");
    expect(drain(silentJob).back().failure == "legendary stopped with status 4",
           "without a reason it names the status");

    EpicInstallJob missing{(dir / "missing").string()};
    expect(missing.start("Game", "Game"), "a missing legendary starts");
    expect(drain(missing).back().failure == "legendary is not installed",
           "a missing legendary says so");

    // Dropping a job stops legendary and the processes it started.
    const fs::path forever = writeStub(dir / "legendary-forever", "sleep 60\n");
    const Clock::time_point began = Clock::now();
    {
        EpicInstallJob running{forever.string()};
        expect(running.start("Game", "Game"), "a long Epic install starts");
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
    }
    expect(Clock::now() - began < std::chrono::seconds{10},
           "a job going away does not wait for legendary");
}

void testInstalls(const fs::path& dir) {
    const fs::path slow = writeStub(dir / "legendary-routed", "sleep 0.3\nexit 0\n");
    iideck::steam::Client steam{iideck::steam::Client::Options{
        .home = dir / "home", .executablePath = {}, .session = "install-test", .steamRoots = {}}};
    Installs installs{steam, slow.string()};

    iideck::library::Game epic;
    epic.source = Source::Epic;
    epic.sourceId = "Fortnite";
    epic.title = "Fortnite";
    iideck::library::Game gog = epic;
    gog.source = Source::Gog;
    expect(!installs.start(gog), "a GOG game is not installed");
    expect(!installs.running(), "and nothing runs for it");
    expect(installs.start(epic) && installs.running(), "an Epic game starts its installer");
    expect(!installs.start(epic), "one install at a time");
    expect(installs.title() == "Fortnite", "the running title");
    const Clock::time_point until = Clock::now() + std::chrono::seconds{20};
    bool finished = false;
    while (!finished && Clock::now() < until) {
        const std::optional<InstallJob::Report> report = installs.take();
        finished = report && report->finished;
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    expect(finished && !installs.running(), "the report comes through the router");
    expect(installs.title() == "Fortnite", "the title outlives the install");
}

void testSupports() {
    expect(Installs::supports(Source::Steam), "Steam installs");
    expect(Installs::supports(Source::Epic), "Epic installs");
    expect(!Installs::supports(Source::Gog), "GOG does not install yet");
    expect(!Installs::supports(Source::Rom), "a ROM is not installed");
}

} // namespace

int main() {
    const fs::path dir = fs::current_path() / ("install-job-test-" + std::to_string(getpid()));
    fs::remove_all(dir);
    testJobLifecycle();
    testStoppedJob();
    testEpic(dir);
    testInstalls(dir);
    testSupports();
    fs::remove_all(dir);
    std::printf("install_job: all checks passed\n");
    return 0;
}
