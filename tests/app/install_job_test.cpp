// Install jobs: the store-neutral job's report handling through a fake installer, the Epic and GOG
// jobs against stub `legendary` and `gogdl` programs, and which stores install.
#include "epic_install_job.hpp"
#include "gog_install_job.hpp"
#include "install_job.hpp"
#include "installs.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

#include "library/gog_installs.hpp"
#include "library/gog_token.hpp"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::app::EpicInstallJob;
using opensu::app::GogInstallJob;
using opensu::app::InstallJob;
using opensu::app::Installs;
using opensu::library::Source;

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

std::string firstLine(const fs::path& file) {
    std::ifstream in{file};
    std::string line;
    std::getline(in, line);
    return line;
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

    EpicInstallJob placed{ok.string()};
    placed.setFolder(dir / "epic-games");
    expect(placed.start("Fortnite", "Fortnite"), "an Epic install into a folder starts");
    expect(drain(placed).back().failure.empty(), "it finishes installed");
    expect(firstLine(dir / "args") ==
               "install Fortnite -y --skip-sdl --base-path " + (dir / "epic-games").string(),
           "legendary installs under the folder it is given");
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

/// A fake gogdl whose `download` records its arguments and the auth file's mode, makes a game
/// folder and logs progress as gogdl does.
fs::path writeGogdl(const fs::path& dir) {
    return writeStub(dir / "gogdl", R"sh(auth=""
if [ "$1" = "--auth-config-path" ]; then auth="$2"; shift 2; fi
grep -q '"access_token":"ACCESS-1"' "$auth" || { echo "[AUTH] ERROR: no token" >&2; exit 3; }
case "$1" in
download)
    id="$2"
    all="$*"
    while [ "$1" != "--path" ]; do shift; done
    path="$2"
    mkdir -p "$path"
    echo "$(stat -c %a "$auth")" > "$path/../../auth-mode"
    echo "$all" > "$path/../../download-args"
    case "$id" in
    full)
        echo "Unable to proceed, Not enough disk space"
        exit 2
        ;;
    esac
    mkdir -p "$path/Game Folder"
    echo "[PROGRESS] INFO: = Progress: 25.00 5/20, Running for: 00:00:01, ETA: 00:00:03" >&2
    sleep 0.3
    echo "[PROGRESS] INFO: = Progress: 100.00 20/20, Running for: 00:00:04, ETA: 00:00:00" >&2
    sleep 0.3
    [ "$id" = rotate ] && sed -i 's/REFRESH-1/REFRESH-9/' "$auth"
    exit 0
    ;;
esac
exit 9
)sh");
}

fs::path gogData(const fs::path& dir, const char* name) {
    const fs::path data = dir / name;
    fs::create_directories(data);
    opensu::library::gog::TokenStore::under(data).save(
        opensu::library::gog::Token{"ACCESS-1", "REFRESH-1", "4242", 4'000'000'000});
    return data;
}

void testGog(const fs::path& dir) {
    const fs::path gogdl = writeGogdl(dir / "bin");

    const fs::path data = gogData(dir, "gog-native");
    GogInstallJob native{GogInstallJob::Options{.dataDir = data, .gogdl = gogdl.string()}};
    native.setBuilds({.windows = true, .linuxNative = true});
    expect(native.start("native", "Native"), "a GOG install starts");
    std::vector<double> seen;
    const std::vector<InstallJob::Report> reports = drain(native);
    for (const InstallJob::Report& report : reports) {
        if (report.fraction) {
            seen.push_back(*report.fraction);
        }
    }
    expect(reports.back().finished && reports.back().failure.empty(), "it finishes installed");
    expect(seen.size() == 2 && seen[0] == 0.25 && seen[1] == 1.0,
           "progress reaches the loop as gogdl logs it");
    expect(firstLine(data / "download-args") ==
               "download native --platform linux --path " + (data / "gog-games/native").string(),
           "a game with a Linux build downloads that build");
    expect(firstLine(data / "auth-mode") == "600", "gogdl read an owner-only token file");
    expect(!fs::exists(data / "gogdl-auth.json"), "the token file is gone afterwards");
    const auto record =
        opensu::library::gog::InstallRecords{data / "gog-installs.json"}.find("native");
    expect(record && record->platform == "linux" &&
               record->path == data / "gog-games/native/Game Folder",
           "the install is recorded with gogdl's folder and platform");

    const fs::path placedData = gogData(dir, "gog-placed");
    const fs::path placedFolder = dir / "gog-store" / "games";
    GogInstallJob placed{GogInstallJob::Options{.dataDir = placedData, .gogdl = gogdl.string()}};
    placed.setBuilds({.windows = true, .linuxNative = true});
    placed.setFolder(placedFolder);
    expect(placed.start("native", "Native"), "a GOG install into a folder starts");
    expect(drain(placed).back().failure.empty(), "it finishes installed");
    expect(firstLine(placedFolder.parent_path() / "download-args") ==
               "download native --platform linux --path " + (placedFolder / "native").string(),
           "gogdl downloads under the folder it is given");
    const auto placedRecord =
        opensu::library::gog::InstallRecords{placedData / "gog-installs.json"}.find("native");
    expect(placedRecord && placedRecord->path == placedFolder / "native/Game Folder",
           "the install is recorded where gogdl put it");

    const fs::path windowsData = gogData(dir, "gog-windows");
    GogInstallJob windows{GogInstallJob::Options{.dataDir = windowsData, .gogdl = gogdl.string()}};
    windows.setBuilds({.windows = true, .linuxNative = false});
    expect(windows.start("winonly", "Win"), "a Windows-only install starts");
    expect(drain(windows).back().failure.empty(), "it finishes installed");
    expect(firstLine(windowsData / "download-args").find("--platform windows") != std::string::npos,
           "a game without a Linux build downloads the Windows build");
    const auto windowsRecord =
        opensu::library::gog::InstallRecords{windowsData / "gog-installs.json"}.find("winonly");
    expect(windowsRecord && windowsRecord->platform == "windows", "and is recorded as Windows");

    // gogdl refreshed the token while it downloaded; opensu keeps the new one.
    const fs::path rotatedData = gogData(dir, "gog-rotated");
    GogInstallJob rotated{GogInstallJob::Options{.dataDir = rotatedData, .gogdl = gogdl.string()}};
    rotated.setBuilds({.windows = true, .linuxNative = true});
    expect(rotated.start("rotate", "Rotate"), "a long install starts");
    expect(drain(rotated).back().failure.empty(), "it finishes installed");
    const auto kept = opensu::library::gog::TokenStore::under(rotatedData).load();
    expect(kept && kept->refreshToken == "REFRESH-9",
           "the refresh token gogdl rotated is saved in opensu's token store");

    const fs::path fullData = gogData(dir, "gog-full");
    GogInstallJob full{GogInstallJob::Options{.dataDir = fullData, .gogdl = gogdl.string()}};
    full.setBuilds({.windows = true, .linuxNative = true});
    expect(full.start("full", "Full"), "an install on a full disk starts");
    expect(drain(full).back().failure == "Not enough disk space", "it fails with gogdl's reason");
    expect(!fs::exists(fullData / "gog-installs.json") && !fs::exists(fullData / "gogdl-auth.json"),
           "a failed install records nothing and leaves no token file");

    const fs::path noneData = gogData(dir, "gog-none");
    GogInstallJob none{GogInstallJob::Options{.dataDir = noneData, .gogdl = gogdl.string()}};
    expect(none.start("native", "Native"), "a game with no listed build starts");
    expect(drain(none).back().failure == "GOG lists no Linux or Windows build of this game",
           "it fails without running gogdl");
    expect(!fs::exists(noneData / "download-args"), "gogdl was not run");

    const fs::path emptyData = dir / "gog-signed-out";
    GogInstallJob signedOut{GogInstallJob::Options{.dataDir = emptyData, .gogdl = gogdl.string()}};
    signedOut.setBuilds({.windows = true, .linuxNative = true});
    expect(signedOut.start("native", "Native"), "a signed-out install starts");
    expect(drain(signedOut).back().failure == "GOG is not signed in", "it asks for a sign-in");

    const fs::path missingData = gogData(dir, "gog-missing");
    GogInstallJob missing{
        GogInstallJob::Options{.dataDir = missingData, .gogdl = (dir / "no-gogdl").string()}};
    missing.setBuilds({.windows = true, .linuxNative = true});
    expect(missing.start("native", "Native"), "a missing gogdl starts");
    expect(drain(missing).back().failure == "gogdl is not installed", "a missing gogdl says so");
}

void testInstalls(const fs::path& dir) {
    const fs::path slow = writeStub(dir / "legendary-routed", "sleep 0.3\nexit 0\n");
    opensu::steam::Client steam{opensu::steam::Client::Options{
        .home = dir / "home", .executablePath = {}, .session = "install-test", .steamRoots = {}}};
    std::vector<Source> asked;
    Installs installs{steam, slow.string(), GogInstallJob::Options{.dataDir = dir / "data"},
                      [&asked](Source store) -> std::optional<fs::path> {
                          asked.push_back(store);
                          return std::nullopt;
                      }};

    opensu::library::Game epic;
    epic.source = Source::Epic;
    epic.sourceId = "Fortnite";
    epic.title = "Fortnite";
    opensu::library::Game rom = epic;
    rom.source = Source::Rom;
    expect(!installs.start(rom), "a ROM is not installed");
    expect(!installs.running(), "and nothing runs for it");
    expect(installs.start(epic) && installs.running(), "an Epic game starts its installer");
    expect(asked == std::vector<Source>{Source::Epic},
           "the store's folder is asked for once, when its install starts");
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

    // The listing's builds reach the GOG job: with one the install goes on to need a sign-in.
    opensu::library::Game gog = epic;
    gog.source = Source::Gog;
    gog.sourceId = "1";
    for (const bool linux : {false, true}) {
        gog.builds = {.windows = false, .linuxNative = linux};
        expect(installs.start(gog), "a GOG install starts");
        std::optional<InstallJob::Report> last;
        const Clock::time_point wait = Clock::now() + std::chrono::seconds{20};
        while ((!last || !last->finished) && Clock::now() < wait) {
            if (std::optional<InstallJob::Report> report = installs.take()) {
                last = report;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        expect(last &&
                   last->failure == (linux ? "GOG is not signed in"
                                           : "GOG lists no Linux or Windows build of this game"),
               "the game's builds decide whether the GOG install can go on");
    }
}

void testFolderRefusal(const fs::path& dir) {
    const fs::path stub = writeStub(dir / "legendary-refusal", "exit 0\n");
    opensu::steam::Client steam{opensu::steam::Client::Options{.home = dir / "home",
                                                               .executablePath = {},
                                                               .session = "install-refusal",
                                                               .steamRoots = {}}};
    const fs::path good = dir / "good-games";
    fs::create_directories(good);
    std::optional<fs::path> chosen;
    Installs installs{steam, stub.string(), GogInstallJob::Options{.dataDir = dir / "data"},
                      [&chosen](Source) {
                          return chosen;
                      }};
    expect(installs.folderRefusal(Source::Epic).empty(), "a store that chooses is not refused");
    chosen = good;
    expect(installs.folderRefusal(Source::Epic).empty(), "a writable folder is accepted");
    chosen = dir / "nowhere";
    expect(!installs.folderRefusal(Source::Gog).empty(),
           "a missing folder is refused with a reason");
    chosen = fs::path{"relative"};
    expect(!installs.folderRefusal(Source::Steam).empty(), "a relative folder is refused");
}

void testSupports() {
    expect(Installs::supports(Source::Steam), "Steam installs");
    expect(Installs::supports(Source::Epic), "Epic installs");
    expect(Installs::supports(Source::Gog), "GOG installs");
    expect(!Installs::supports(Source::Rom), "a ROM is not installed");
}

} // namespace

int main() {
    const fs::path dir = fs::current_path() / ("install-job-test-" + std::to_string(getpid()));
    fs::remove_all(dir);
    testJobLifecycle();
    testStoppedJob();
    testEpic(dir);
    testGog(dir);
    testInstalls(dir);
    testFolderRefusal(dir);
    testSupports();
    fs::remove_all(dir);
    std::printf("install_job: all checks passed\n");
    return 0;
}
