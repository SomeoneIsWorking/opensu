// Updates: `Installs` routes an update to its store's updater (`legendary update`, `gogdl update`
// over the recorded install, Steam's ResumeAppUpdate), the GOG build gogdl leaves is recorded, and
// an update is refused as an uninstall is and shares the one-at-a-time rule.
#include "installs.hpp"
#include "steam_update_job.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>

#include "library/gog_installs.hpp"
#include "library/gog_token.hpp"
#include "support/fake_steam.hpp"
#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::app::GogInstallJob;
using opensu::app::InstallJob;
using opensu::app::Installs;
using opensu::app::SteamUpdateJob;
using opensu::library::Game;
using opensu::library::Source;
using opensu::library::gog::Installed;
using opensu::library::gog::InstallRecords;

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

std::string line(const fs::path& file, int index) {
    std::ifstream in{file};
    std::string text;
    for (int at = 0; at <= index; ++at) {
        if (!std::getline(in, text)) {
            return {};
        }
    }
    return text;
}

template <typename Jobs> InstallJob::Report finish(Jobs& jobs) {
    const Clock::time_point until = Clock::now() + std::chrono::seconds{20};
    while (Clock::now() < until) {
        if (std::optional<InstallJob::Report> report = jobs.take(); report && report->finished) {
            return *report;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    expect(false, "the job finished in time");
    return {};
}

opensu::steam::Client stoppedSteam(const fs::path& dir) {
    return opensu::steam::Client{opensu::steam::Client::Options{
        .home = dir / "home", .executablePath = {}, .session = "update-test", .steamRoots = {}}};
}

Game updatable(Source source, const std::string& id, const std::string& title) {
    Game made;
    made.id = std::string{opensu::library::label(source)} + ":" + id;
    made.source = source;
    made.sourceId = id;
    made.title = title;
    made.installed = true;
    made.updateAvailable = true;
    return made;
}

Installs::Folders noFolders() {
    return [](Source) -> std::optional<fs::path> {
        return std::nullopt;
    };
}

void testEpic(const fs::path& dir) {
    opensu::steam::Client steam{stoppedSteam(dir)};
    const fs::path legendary =
        writeStub(dir / "legendary", R"(echo "$@" >> ')" + (dir / "calls").string() + R"('
echo "[DLManager] INFO: = Progress: 50.00% (2/4), Running for 00:00:01, ETA: 00:00:01" >&2
sleep 0.3
exit 0
)");
    std::string playing;
    Installs installs{steam, legendary.string(), GogInstallJob::Options{.dataDir = dir / "data"},
                      noFolders(), [&playing](const Game& game) {
                          return game.id == playing;
                      }};
    const Game fortnite = updatable(Source::Epic, "Fortnite", "Fortnite");

    Game rom = fortnite;
    rom.source = Source::Rom;
    expect(installs.update(rom) == "openSU does not update ROMs", "a ROM is never updated");
    Game absent = fortnite;
    absent.installed = false;
    expect(installs.update(absent) == "Fortnite is not installed",
           "a game that is not installed is not updated");
    playing = fortnite.id;
    expect(installs.update(fortnite) == "Fortnite is running" && !installs.running(),
           "a game that runs is not updated");
    playing.clear();
    Game current = fortnite;
    current.updateAvailable = false;
    expect(installs.update(current) == "Fortnite is up to date",
           "a game with no update known is not updated");

    expect(installs.update(fortnite).empty(), "the update starts");
    const std::optional<Installs::Status> status = installs.status();
    expect(status && status->work == Installs::Work::Update && status->store == Source::Epic &&
               status->job.appId == "Fortnite",
           "its state says it is an Epic update");
    Game other = updatable(Source::Epic, "Other", "Other");
    other.installed = false;
    expect(!installs.start(other) && installs.busy() == "Fortnite is still updating",
           "no install starts while an update runs");
    expect(installs.uninstall(fortnite) == "Fortnite is still updating" &&
               installs.update(fortnite) == "Fortnite is still updating",
           "nor an uninstall or a second update");
    const InstallJob::Report end = finish(installs);
    expect(end.failure.empty() && !end.cancelled, "the update ends done");
    expect(line(dir / "calls", 0) == "update Fortnite --update-only -y --skip-sdl",
           "it runs `legendary update <app> --update-only -y --skip-sdl`");

    expect(installs.start(other), "an install starts after it");
    expect(finish(installs).failure.empty(), "and ends");
    expect(line(dir / "calls", 1) == "install Other -y --skip-sdl",
           "the next install is an install again");
}

/// A fake gogdl: `download` makes the game folder with a goggame file, `update` records its
/// arguments and rewrites the goggame file with a newer build.
fs::path writeGogdl(const fs::path& dir) {
    return writeStub(dir / "gogdl", R"sh(auth=""
if [ "$1" = "--auth-config-path" ]; then auth="$2"; shift 2; fi
grep -q '"access_token":"ACCESS-1"' "$auth" || { echo "[AUTH] ERROR: no token" >&2; exit 3; }
command="$1"
id="$2"
all="$*"
while [ "$1" != "--path" ]; do shift; done
path="$2"
case "$command" in
download)
    mkdir -p "$path/Game $id"
    echo '{"buildId":"B-'"$id"'","gameId":"'"$id"'","rootGameId":"'"$id"'"}' > "$path/Game $id/goggame-$id.info"
    ;;
update)
    echo "$all" > "$path/../../../update-args"
    echo '{"buildId":"NEW","gameId":"'"$id"'","rootGameId":"'"$id"'"}' > "$path/goggame-$id.info"
    ;;
*) exit 9 ;;
esac
exit 0
)sh");
}

void testGog(const fs::path& dir) {
    opensu::steam::Client steam{stoppedSteam(dir)};
    const fs::path data = dir / "data";
    fs::create_directories(data);
    opensu::library::gog::TokenStore::under(data).save(
        opensu::library::gog::Token{"ACCESS-1", "REFRESH-1", "4242", 4'000'000'000});
    const fs::path gogdl = writeGogdl(dir / "bin");
    Installs installs{steam, "legendary",
                      GogInstallJob::Options{.dataDir = data, .gogdl = gogdl.string()}, noFolders(),
                      [](const Game&) {
                          return false;
                      }};
    const InstallRecords records{data / "gog-installs.json"};

    Game eight = updatable(Source::Gog, "8", "Eight");
    eight.installed = false;
    eight.builds.windows = true;
    expect(installs.start(eight) && finish(installs).failure.empty(), "a GOG install ends");
    const std::optional<Installed> installed = records.find("8");
    expect(installed && installed->build == "B-8",
           "an install records the build gogdl left in the game's folder");

    expect(installs.update(updatable(Source::Gog, "7", "Seven")) ==
               "openSU has no record of the GOG install",
           "a GOG game with no record is not updated");
    const fs::path seven = data / "gog-games/7/Seven";
    fs::create_directories(seven);
    std::ofstream{seven / "goggame-7.info"} << R"({"buildId":"OLD","gameId":"7"})";
    records.add("7", Installed{.path = seven, .platform = "windows", .build = "OLD"});
    expect(installs.update(updatable(Source::Gog, "7", "Seven")).empty(), "the update starts");
    const InstallJob::Report end = finish(installs);
    expect(end.failure.empty(), "the update ends done");
    expect(line(data / "update-args", 0) == "update 7 --platform windows --path " + seven.string(),
           "it runs `gogdl update` on the recorded folder and platform");
    const std::optional<Installed> updated = records.find("7");
    expect(updated && updated->path == seven && updated->platform == "windows" &&
               updated->build == "NEW",
           "the record keeps its folder and takes the new build");
    expect(!fs::exists(data / "gogdl-auth.json"), "the token file is gone afterwards");
}

void testSteam(const fs::path& dir) {
    opensu::steam::Client stopped{stoppedSteam(dir)};
    Installs installs{stopped, "legendary", GogInstallJob::Options{.dataDir = dir / "data"},
                      noFolders(), [](const Game&) {
                          return false;
                      }};
    expect(installs.update(updatable(Source::Steam, "1628350", "Deep Game")) ==
                   "Steam is not running" &&
               !installs.running(),
           "a Steam update is refused while Steam is not running");

    opensu::test::FakeSteam fake{[](const std::string&) {
        return nlohmann::json{};
    }};
    opensu::steam::DevTools devTools{fake.port()};
    opensu::steam::DownloadControl control{devTools};
    bool ready = true;
    SteamUpdateJob job{[&ready] {
                           return ready;
                       },
                       control};
    expect(job.start("1628350", "Deep Game"), "a Steam update starts");
    const InstallJob::Report end = finish(job);
    expect(end.failure.empty() && end.line == "update queued in Steam",
           "it ends once Steam has the update queued");
    expect(fake.calls().back() == "SteamClient.Downloads.ResumeAppUpdate(1628350, \"0\")",
           "it starts the update as Steam's own Update button does");

    ready = false;
    const std::size_t calls = fake.calls().size();
    expect(job.start("1628350", "Deep Game") && finish(job).failure == "Steam is not running" &&
               fake.calls().size() == calls,
           "without Steam it fails and asks nothing");
}

} // namespace

int main() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "update-job"};
    testEpic(scratch.path() / "epic");
    testGog(scratch.path() / "gog");
    testSteam(scratch.path() / "steam");
    std::printf("update_job: all checks passed\n");
    return 0;
}
