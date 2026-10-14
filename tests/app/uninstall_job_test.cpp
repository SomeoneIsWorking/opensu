// Uninstalls: the Epic job against a stub `legendary`, the GOG job's folder rule on scratch
// folders, a Steam uninstall without Steam, and the refusals and one-at-a-time rule `Installs`
// keeps across installs and uninstalls.
#include "installs.hpp"
#include "uninstall_job.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "library/gog_installs.hpp"
#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::app::GogInstallJob;
using opensu::app::InstallJob;
using opensu::app::Installs;
using opensu::app::UninstallJob;
using opensu::library::Game;
using opensu::library::Source;
using opensu::library::gog::Installed;
using opensu::library::gog::InstallRecords;
using opensu::library::gog::Paths;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
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

/// The job's last report once it finishes, taken as the loop would.
InstallJob::Report finish(InstallJob& job) {
    const Clock::time_point until = Clock::now() + std::chrono::seconds{20};
    while (Clock::now() < until) {
        if (std::optional<InstallJob::Report> report = job.take(); report && report->finished) {
            return *report;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    expect(false, "the job finished in time");
    return {};
}

/// The same, through `Installs`.
InstallJob::Report finish(Installs& installs) {
    const Clock::time_point until = Clock::now() + std::chrono::seconds{20};
    while (Clock::now() < until) {
        if (std::optional<InstallJob::Report> report = installs.take();
            report && report->finished) {
            return *report;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    expect(false, "the job finished in time");
    return {};
}

opensu::steam::Client stoppedSteam(const fs::path& dir) {
    return opensu::steam::Client{opensu::steam::Client::Options{
        .home = dir / "home", .executablePath = {}, .session = "uninstall-test", .steamRoots = {}}};
}

Game game(Source source, const std::string& id, const std::string& title) {
    Game made;
    made.id = std::string{opensu::library::label(source)} + ":" + id;
    made.source = source;
    made.sourceId = id;
    made.title = title;
    made.installed = true;
    return made;
}

void testEpic(const fs::path& dir) {
    opensu::steam::Client steam{stoppedSteam(dir)};
    const fs::path ok =
        writeStub(dir / "legendary-ok", R"(echo "$@" > ')" + (dir / "args").string() + R"('
echo "[cli] INFO: Removing game files..." >&2
exit 0
)");
    UninstallJob job{steam, ok.string(), dir / "data"};
    job.setStore(Source::Epic, std::nullopt);
    expect(job.removal(game(Source::Epic, "Fortnite", "Fortnite"), std::nullopt).what ==
               "legendary deletes the Epic copy",
           "the confirm names what legendary removes");
    expect(job.start("Fortnite", "Fortnite"), "an Epic uninstall starts");
    const InstallJob::Report end = finish(job);
    expect(end.failure.empty() && end.line == "uninstalled", "it ends uninstalled");
    expect(firstLine(dir / "args") == "uninstall Fortnite -y",
           "it runs `legendary uninstall <app> -y`");
    const std::optional<InstallJob::Status> status = job.status();
    expect(status && status->phase == InstallJob::Phase::Done, "its state is done");

    const fs::path failing =
        writeStub(dir / "legendary-fails", R"(echo "[cli] ERROR: Game is not installed" >&2
exit 1
)");
    UninstallJob failed{steam, failing.string(), dir / "data"};
    failed.setStore(Source::Epic, std::nullopt);
    expect(failed.start("Fortnite", "Fortnite"), "a failing uninstall starts");
    expect(finish(failed).failure == "Game is not installed", "it fails with legendary's reason");

    UninstallJob missing{steam, (dir / "no-legendary").string(), dir / "data"};
    missing.setStore(Source::Epic, std::nullopt);
    expect(missing.start("Fortnite", "Fortnite"), "an uninstall without legendary starts");
    expect(finish(missing).failure == "legendary is not installed", "it says legendary is missing");
}

void testGog(const fs::path& dir) {
    opensu::steam::Client steam{stoppedSteam(dir)};
    const fs::path data = dir / "data";
    const Paths paths = Paths::under(data);
    const InstallRecords records{paths.records};
    const fs::path folder = dir / "gog-folder";

    // Inside the root openSU gave gogdl: the folder goes, then the record; the prefix stays.
    const fs::path inside = paths.gameRoot(folder, "1") / "Game One";
    fs::create_directories(inside / "bin");
    std::ofstream{inside / "bin" / "game"} << "x";
    fs::create_directories(paths.prefixes / "1" / "drive_c");
    records.add("1", Installed{.path = inside, .platform = "windows"});
    UninstallJob job{steam, "legendary", data};
    const Game one = game(Source::Gog, "1", "One");
    expect(job.removal(one, folder).what == "deletes the GOG copy at " + inside.string(),
           "the confirm names the folder it deletes");
    job.setStore(Source::Gog, folder);
    expect(job.start("1", "One"), "a GOG uninstall starts");
    const InstallJob::Report removed = finish(job);
    expect(removed.failure.empty() && removed.line == "uninstalled", "it ends uninstalled");
    expect(!fs::exists(inside) && !fs::exists(paths.gameRoot(folder, "1")),
           "the game's folder and its emptied root are deleted");
    expect(!records.find("1"), "the record is forgotten");
    expect(fs::exists(paths.prefixes / "1" / "drive_c"), "the Wine prefix is kept");

    // Outside the root: forgotten, nothing deleted.
    const fs::path outside = dir / "own-games" / "Game Two";
    fs::create_directories(outside);
    records.add("2", Installed{.path = outside, .platform = "linux"});
    const Game two = game(Source::Gog, "2", "Two");
    expect(job.removal(two, folder).what ==
               "forgets the GOG copy; " + outside.string() + " is kept",
           "the confirm says a folder outside the root is kept");
    job.setStore(Source::Gog, folder);
    expect(job.start("2", "Two"), "an uninstall of a game outside the root starts");
    const InstallJob::Report forgotten = finish(job);
    expect(forgotten.failure.empty() && forgotten.line.starts_with("forgotten; "),
           "it ends forgotten");
    expect(fs::exists(outside) && !records.find("2"), "the folder stays and the record goes");

    // A record in the root that is a symlink out of it: only forgotten, the target untouched.
    const fs::path target = dir / "precious";
    fs::create_directories(target);
    std::ofstream{target / "save"} << "x";
    fs::create_directories(paths.gameRoot(folder, "3"));
    const fs::path link = paths.gameRoot(folder, "3") / "Game Three";
    fs::create_directory_symlink(target, link);
    records.add("3", Installed{.path = link, .platform = "linux"});
    job.setStore(Source::Gog, folder);
    expect(job.start("3", "Three"), "an uninstall of a symlinked game starts");
    expect(finish(job).line.starts_with("forgotten; "), "a symlink out of the root is forgotten");
    expect(fs::exists(target / "save") && !records.find("3"),
           "the folder it pointed at is untouched and the record goes");

    // The root under openSU's data directory when no folder is set.
    const fs::path own = paths.gameRoot(std::nullopt, "4") / "Game Four";
    fs::create_directories(own);
    records.add("4", Installed{.path = own, .platform = "linux"});
    job.setStore(Source::Gog, std::nullopt);
    expect(job.start("4", "Four"), "an uninstall from the data directory starts");
    expect(finish(job).line == "uninstalled" && !fs::exists(own),
           "a game under the data directory is deleted");

    expect(job.removal(game(Source::Gog, "5", "Five"), folder).refusal ==
               "openSU has no record of the GOG install",
           "a game with no record cannot be uninstalled");
}

void testSteamWithoutSteam(const fs::path& dir) {
    opensu::steam::Client steam{stoppedSteam(dir)};
    UninstallJob job{steam, "legendary", dir / "data"};
    expect(job.removal(game(Source::Steam, "480", "Spacewar"), std::nullopt).refusal ==
               "Steam is not running",
           "a Steam game is not uninstalled while Steam is not running");
    job.setStore(Source::Steam, std::nullopt);
    expect(job.start("480", "Spacewar"), "a Steam uninstall starts");
    expect(finish(job).failure == "Steam is not running", "it fails without asking Steam");
}

void testInstallsRefuse(const fs::path& dir) {
    opensu::steam::Client steam{stoppedSteam(dir)};
    const fs::path slow =
        writeStub(dir / "legendary-slow", R"(echo "$@" >> ')" + (dir / "calls").string() + R"('
sleep 0.5
exit 0
)");
    std::string playing;
    Installs installs{steam, slow.string(), GogInstallJob::Options{.dataDir = dir / "data"},
                      [](Source) -> std::optional<fs::path> {
                          return std::nullopt;
                      },
                      [&playing](const Game& game) {
                          return game.id == playing;
                      }};
    const Game epic = game(Source::Epic, "Fortnite", "Fortnite");

    Game rom = epic;
    rom.source = Source::Rom;
    expect(!installs.removal(rom).refusal.empty() && !installs.uninstall(rom).empty(),
           "a ROM is never uninstalled");
    Game absent = epic;
    absent.installed = false;
    expect(installs.uninstall(absent) == "Fortnite is not installed",
           "a game that is not installed is not uninstalled");
    playing = epic.id;
    expect(installs.uninstall(epic) == "Fortnite is running" && !installs.running(),
           "a game that runs is not uninstalled");
    playing.clear();

    Game other = epic;
    other.sourceId = "Other";
    other.title = "Other";
    other.installed = false;
    expect(installs.start(other), "an install starts");
    expect(installs.uninstall(epic) == "Other is still installing",
           "nothing is uninstalled while an install runs");
    other.installed = true;
    expect(installs.removal(other).refusal == "Other is still installing",
           "the game being installed is not uninstalled either");
    expect(finish(installs).failure.empty(), "the install ends");

    expect(installs.removal(epic).what == "legendary deletes the Epic copy",
           "with nothing running the confirm says what goes");
    expect(installs.uninstall(epic).empty(), "the uninstall starts");
    const std::optional<Installs::Status> running = installs.status();
    expect(running && running->work == Installs::Work::Uninstall &&
               running->store == Source::Epic && running->job.appId == "Fortnite",
           "its state says it is an uninstall");
    expect(!installs.start(other) && installs.busy() == "Fortnite is still uninstalling",
           "no install starts while an uninstall runs");
    expect(installs.uninstall(epic) == "Fortnite is still uninstalling",
           "and no second uninstall either");
    expect(finish(installs).line == "uninstalled", "the uninstall's end comes through Installs");
    expect(installs.busy().empty(), "then nothing runs");
    std::ifstream calls{dir / "calls"};
    std::string install;
    std::string uninstall;
    std::getline(calls, install);
    std::getline(calls, uninstall);
    expect(install.starts_with("install Other") && uninstall == "uninstall Fortnite -y",
           "each job ran its own legendary command");
}

} // namespace

int main() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "uninstall-job"};
    testEpic(scratch.path() / "epic");
    testGog(scratch.path() / "gog");
    testSteamWithoutSteam(scratch.path() / "steam");
    testInstallsRefuse(scratch.path() / "installs");
    std::printf("uninstall_job: all checks passed\n");
    return 0;
}
