// The Downloads page's model: every source merged into one list, ordered, with each owner's state
// and actions, and ended downloads kept.
#include "downloads_board.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using opensu::app::DownloadAction;
using opensu::app::DownloadKind;
using opensu::app::DownloadRow;
using opensu::app::DownloadsBoard;
using opensu::app::DownloadState;
using opensu::app::EmulatorEnd;
using opensu::app::EmulatorLook;
using opensu::app::InstallJob;
using opensu::app::Installs;
using opensu::library::Source;
using opensu::steam::Download;
using Phase = InstallJob::Phase;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

DownloadsBoard board() {
    return DownloadsBoard{[](Source store, const std::string& id) -> std::string {
        if (store == Source::Steam && id == "480") {
            return "Spacewar";
        }
        if (store == Source::Steam && id == "70") {
            return "Half-Life";
        }
        return {};
    }};
}

std::vector<std::string> keys(const DownloadsBoard& shown) {
    std::vector<std::string> out;
    for (const DownloadRow& row : shown.rows()) {
        out.push_back(row.key);
    }
    return out;
}

Installs::Status install(Source store, std::string appId, Phase phase, std::uint64_t serial = 1) {
    return Installs::Status{
        .store = store,
        .job = InstallJob::Status{
            .serial = serial, .appId = std::move(appId), .title = "Some Game", .phase = phase}};
}

void testMergesEverySource() {
    DownloadsBoard shown = board();
    Installs::Status epic = install(Source::Epic, "Fortnite", Phase::Running);
    epic.job.fraction = 0.42;
    epic.job.line = "Installing · 42%";
    shown.update({.steamReady = true,
                  .steam = {Download{.appId = "480",
                                     .progress = 0.5,
                                     .active = true,
                                     .queueIndex = 0,
                                     .bytesDone = 5,
                                     .bytesTotal = 10,
                                     .bytesPerSecond = 2,
                                     .updateState = "Downloading"},
                            Download{.appId = "999", .queueIndex = 1}},
                  .install = epic,
                  .emulator = EmulatorLook{.id = "mgba",
                                           .name = "mGBA",
                                           .kind = DownloadKind::Core,
                                           .received = 25,
                                           .total = 100}});
    expect(shown.rows().size() == 4, "one row per download from every source");
    const DownloadRow* steam = shown.find("steam:480");
    expect(steam != nullptr && steam->title == "Spacewar" && steam->store == Source::Steam &&
               steam->state == DownloadState::Downloading && steam->fraction == 0.5 &&
               steam->bytesDone == 5 && steam->bytesTotal == 10 && steam->bytesPerSecond == 2u,
           "Steam's active download carries its title and figures");
    expect(shown.find("steam:999")->title == "Steam app 999",
           "a Steam app the library does not know is named by its id");
    const DownloadRow* store = shown.find("epic:Fortnite");
    expect(store != nullptr && store->title == "Some Game" &&
               store->state == DownloadState::Downloading && store->fraction == 0.42,
           "the store install is listed with its measure");
    const DownloadRow* core = shown.find("emulator:mgba");
    expect(core != nullptr && core->kind == DownloadKind::Core && core->title == "mGBA" &&
               core->fraction == 0.25 && core->state == DownloadState::Downloading,
           "the emulator download is listed with its bytes");
}

void testOrdering() {
    DownloadsBoard shown = board();
    shown.update(
        {.steamReady = true,
         .steam = {Download{.appId = "3", .queueIndex = 3}, Download{.appId = "1", .queueIndex = 1},
                   Download{.appId = "9", .paused = true, .queueIndex = -1},
                   Download{.appId = "2", .queueIndex = 2},
                   Download{.appId = "480", .active = true, .queueIndex = 0}}});
    expect(keys(shown) ==
               std::vector<std::string>{"steam:480", "steam:9", "steam:1", "steam:2", "steam:3"},
           "downloading, then paused, then the queue in Steam's order");

    shown.update(
        {.steamReady = true,
         .steam = {Download{.appId = "3", .queueIndex = 1}, Download{.appId = "2", .queueIndex = 2},
                   Download{.appId = "9", .paused = true, .queueIndex = -1},
                   Download{.appId = "1", .active = true, .queueIndex = 0}}});
    expect(keys(shown) ==
               std::vector<std::string>{"steam:1", "steam:9", "steam:3", "steam:2", "steam:480"},
           "the queue follows Steam's new order and a finished download goes after it");
    expect(shown.find("steam:480")->state == DownloadState::Finished &&
               shown.find("steam:480")->fraction == 1.0,
           "a download Steam takes off its queue has finished");

    shown.update({.steamReady = true, .steam = {Download{.appId = "9", .paused = true}}});
    expect(keys(shown) ==
               std::vector<std::string>{"steam:9", "steam:1", "steam:2", "steam:3", "steam:480"},
           "ended downloads stay listed, latest first");
}

void testSteamStates() {
    DownloadsBoard shown = board();
    shown.update(
        {.steamReady = true,
         .steam = {
             Download{.appId = "1", .active = true, .queueIndex = 0, .updateState = "Verifying"},
             Download{.appId = "2", .paused = true, .queueIndex = -1},
             Download{.appId = "3", .queueIndex = 1}}});
    expect(shown.find("steam:1")->state == DownloadState::Installing,
           "Steam past the transfer is installing");
    expect(shown.find("steam:1")->update, "an active download not marked install is an update");
    expect(shown.find("steam:2")->state == DownloadState::Paused,
           "a download out of the queue is paused");
    expect(shown.find("steam:3")->state == DownloadState::Queued, "a waiting download is queued");

    shown.update({.steamReady = true,
                  .steam = {Download{.appId = "1", .paused = true, .queueIndex = 0},
                            Download{.appId = "3", .queueIndex = 1}}});
    expect(shown.find("steam:1")->state == DownloadState::Paused,
           "the head of a paused queue is paused");
}

void testSteamActions() {
    DownloadsBoard shown = board();
    shown.update(
        {.steamReady = true,
         .steam = {Download{.appId = "1", .active = true, .queueIndex = 0},
                   Download{.appId = "2", .queueIndex = 1}, Download{.appId = "3", .queueIndex = 2},
                   Download{.appId = "4", .queueIndex = 3},
                   Download{.appId = "5", .paused = true, .queueIndex = -1}}});
    using A = DownloadAction;
    expect(actionsFor(*shown.find("steam:1")) == std::vector{A::Pause},
           "the active download pauses the queue, and is not removed while it moves");
    expect(actionsFor(*shown.find("steam:2")) == std::vector{A::MoveToTop, A::MoveDown, A::Remove},
           "the next in line goes to the top or down");
    expect(actionsFor(*shown.find("steam:3")) ==
               std::vector{A::MoveToTop, A::MoveUp, A::MoveDown, A::Remove},
           "the middle of the queue moves both ways");
    expect(actionsFor(*shown.find("steam:4")) == std::vector{A::MoveToTop, A::MoveUp, A::Remove},
           "the last in line does not move down");
    expect(actionsFor(*shown.find("steam:5")) == std::vector{A::MoveToTop, A::Remove},
           "a paused download out of the queue is started or removed");

    shown.update(
        {.steamReady = true, .steam = {Download{.appId = "1", .paused = true, .queueIndex = 0}}});
    expect(actionsFor(*shown.find("steam:1")) == std::vector{A::Resume, A::Remove},
           "the paused head resumes the queue or is removed");
    expect(actionsFor(*shown.find("steam:2")).empty(), "a finished download has no actions");
}

void testInstallStates() {
    DownloadsBoard shown = board();
    shown.update({.install = install(Source::Gog, "1207", Phase::Running)});
    const DownloadRow* waiting = shown.find("gog:1207");
    expect(waiting != nullptr && waiting->state == DownloadState::Queued &&
               actionsFor(*waiting) == std::vector{DownloadAction::Cancel},
           "an install not yet measured is queued and can be cancelled");
    Installs::Status measured = install(Source::Gog, "1207", Phase::Running);
    measured.job.fraction = 0.1;
    shown.update({.install = measured});
    expect(shown.find("gog:1207")->state == DownloadState::Downloading,
           "a measured install is downloading");

    Installs::Status failed = install(Source::Gog, "1207", Phase::Failed);
    failed.job.failure = "Not enough disk space";
    failed.job.line = "Install failed";
    shown.update({.install = failed});
    const DownloadRow* over = shown.find("gog:1207");
    expect(over->state == DownloadState::Failed && over->detail == "Not enough disk space" &&
               actionsFor(*over).empty(),
           "a failed install says why and has no actions");

    shown.update({.install = install(Source::Epic, "Fortnite", Phase::Cancelled, 2)});
    expect(shown.find("epic:Fortnite")->state == DownloadState::Cancelled &&
               shown.find("gog:1207")->state == DownloadState::Failed,
           "each install keeps its own ending");
    expect(keys(shown) == std::vector<std::string>{"epic:Fortnite", "gog:1207"},
           "the latest ending comes first");

    shown.update({.install = install(Source::Epic, "Fortnite", Phase::Done, 3)});
    expect(shown.find("epic:Fortnite")->state == DownloadState::Finished &&
               shown.find("epic:Fortnite")->fraction == 1.0,
           "the same game installed again is listed once, finished");
}

void testUninstallStates() {
    DownloadsBoard shown = board();
    Installs::Status removing = install(Source::Gog, "1207", Phase::Running, 4);
    removing.work = Installs::Work::Uninstall;
    removing.job.line = "Removing its files";
    shown.update({.install = removing});
    const DownloadRow* row = shown.find("gog:1207");
    expect(row != nullptr && row->state == DownloadState::Uninstalling && !row->fraction &&
               row->detail == "Removing its files",
           "a running uninstall is listed as uninstalling");
    expect(actionsFor(*row).empty(), "an uninstall has no Cancel");

    removing.job.phase = Phase::Done;
    shown.update({.install = removing});
    row = shown.find("gog:1207");
    expect(row->state == DownloadState::Uninstalled && !row->fraction && ended(row->state) &&
               actionsFor(*row).empty(),
           "a done uninstall ends uninstalled, with no bar and no buttons");
    expect(spelling(DownloadState::Uninstalling) == "uninstalling" &&
               spelling(DownloadState::Uninstalled) == "uninstalled",
           "both states have a spelling for the control channel");

    removing.job.phase = Phase::Failed;
    removing.job.failure = "cannot delete it";
    removing.job.serial = 5;
    shown.update({.install = removing});
    expect(shown.find("gog:1207")->state == DownloadState::Failed &&
               shown.find("gog:1207")->detail == "cannot delete it",
           "a failed uninstall says why");
}

void testUpdateStates() {
    DownloadsBoard shown = board();
    Installs::Status updating = install(Source::Epic, "Fortnite", Phase::Running, 6);
    updating.work = Installs::Work::Update;
    updating.job.fraction = 0.5;
    shown.update({.install = updating});
    const DownloadRow* row = shown.find("epic:Fortnite");
    expect(row != nullptr && row->update && row->state == DownloadState::Downloading &&
               actionsFor(*row) == std::vector{DownloadAction::Cancel},
           "a store update openSU runs is listed as an update with Cancel");
    updating.job.phase = Phase::Done;
    shown.update({.install = updating});
    expect(shown.find("epic:Fortnite")->state == DownloadState::Finished,
           "a done update ends finished");

    // A Steam update is Steam's to run: once queued, Steam's own row carries it.
    Installs::Status queued = install(Source::Steam, "480", Phase::Running, 7);
    queued.work = Installs::Work::Update;
    shown.update({.steamReady = true, .install = queued});
    expect(shown.find("steam:480") != nullptr, "asking Steam shows the row");
    queued.job.phase = Phase::Done;
    shown.update({.steamReady = true, .install = queued});
    expect(shown.find("steam:480") == nullptr,
           "a queued Steam update is not shown finished before Steam lists it");
    shown.update({.steamReady = true,
                  .steam = {Download{.appId = "480", .active = true, .queueIndex = 0}},
                  .install = queued});
    expect(shown.find("steam:480") != nullptr &&
               shown.find("steam:480")->state == DownloadState::Downloading &&
               shown.find("steam:480")->inSteamQueue,
           "Steam's queue row carries it");
    shown.update({.steamReady = true, .install = queued});
    expect(shown.find("steam:480")->state == DownloadState::Finished,
           "and finishes when Steam's queue lets it go");
}

void testSteamInstallJoinsItsQueueRow() {
    DownloadsBoard shown = board();
    shown.update({.steamReady = true, .install = install(Source::Steam, "480", Phase::Licence)});
    expect(shown.find("steam:480")->state == DownloadState::Queued &&
               actionsFor(*shown.find("steam:480")) == std::vector{DownloadAction::Cancel},
           "Steam's installer before its download is queued is cancelled through openSU");
    shown.update({.steamReady = true,
                  .steam = {Download{.appId = "480", .active = true, .queueIndex = 0}},
                  .install = install(Source::Steam, "480", Phase::Done)});
    expect(shown.rows().size() == 1 &&
               shown.find("steam:480")->state == DownloadState::Downloading &&
               shown.find("steam:480")->inSteamQueue,
           "once Steam queues it the install and the download are one row");
    shown.update({.steamReady = true, .install = install(Source::Steam, "480", Phase::Done)});
    expect(shown.find("steam:480")->state == DownloadState::Finished,
           "the row finishes when Steam's queue lets it go");
}

void testSteamUnreadable() {
    DownloadsBoard shown = board();
    shown.update({.steamReady = true, .steam = {Download{.appId = "480", .queueIndex = 1}}});
    shown.update({.steamReady = false});
    expect(shown.rows().empty(), "a Steam download is dropped, not finished, when Steam goes");
}

void testRemoved() {
    DownloadsBoard shown = board();
    const std::vector<Download> queue{Download{.appId = "480", .active = true, .queueIndex = 0},
                                      Download{.appId = "70", .queueIndex = 1}};
    shown.update({.steamReady = true, .steam = queue});
    shown.removed("steam:70");
    expect(shown.find("steam:70") == nullptr, "a removed download leaves the list at once");
    shown.update({.steamReady = true, .steam = queue});
    expect(shown.find("steam:70") == nullptr, "and stays off while Steam still lists it");
    shown.update({.steamReady = true, .steam = {queue[0]}});
    shown.update({.steamReady = true, .steam = queue});
    expect(shown.find("steam:70") != nullptr && shown.find("steam:70")->title == "Half-Life",
           "queued again after Steam let it go, it is listed again");
}

void testEmulatorEnds() {
    DownloadsBoard shown = board();
    shown.update({.emulator = EmulatorLook{.id = "ppsspp", .name = "PPSSPP", .total = 0}});
    const DownloadRow* unsized = shown.find("emulator:ppsspp");
    expect(unsized->kind == DownloadKind::Emulator && !unsized->fraction &&
               unsized->state == DownloadState::Downloading &&
               actionsFor(*unsized) == std::vector{DownloadAction::Cancel},
           "an emulator of unknown size downloads unmeasured and can be cancelled");
    shown.update(
        {.emulator = EmulatorLook{.id = "ppsspp", .name = "PPSSPP", .received = 10, .total = 10}});
    expect(shown.find("emulator:ppsspp")->state == DownloadState::Installing,
           "all bytes in, it is being installed");
    const EmulatorEnd end{.serial = 1,
                          .id = "ppsspp",
                          .name = "PPSSPP",
                          .state = DownloadState::Cancelled,
                          .text = "PPSSPP download cancelled"};
    shown.update({.emulatorEnded = end});
    expect(shown.find("emulator:ppsspp")->state == DownloadState::Cancelled,
           "a cancelled emulator download is listed as cancelled");
    shown.update(
        {.emulator = EmulatorLook{.id = "ppsspp", .name = "PPSSPP"}, .emulatorEnded = end});
    expect(shown.find("emulator:ppsspp")->state == DownloadState::Downloading,
           "downloading it again revives the row, whatever ended before");
    shown.update({.emulatorEnded = end});
    expect(shown.find("emulator:ppsspp")->state == DownloadState::Downloading,
           "an old ending is not applied twice");
    shown.update({.emulatorEnded = EmulatorEnd{.serial = 2,
                                               .id = "ppsspp",
                                               .name = "PPSSPP",
                                               .state = DownloadState::Failed,
                                               .text = "PPSSPP was not installed: no space"}});
    expect(shown.find("emulator:ppsspp")->state == DownloadState::Failed &&
               shown.find("emulator:ppsspp")->detail == "PPSSPP was not installed: no space",
           "a new ending is");
}

void testLabels() {
    expect(label(DownloadAction::MoveToTop) == "Download now" &&
               label(DownloadAction::Cancel) == "Cancel",
           "actions read as Steam's page names them");
    expect(spelling(DownloadState::Installing) == "installing" &&
               label(DownloadState::Installing) == "Installing",
           "states have a label and a spelling");
    expect(ended(DownloadState::Cancelled) && !ended(DownloadState::Paused), "what is over");
}

void testRomsetLists() {
    using opensu::app::RomsetListLook;
    DownloadsBoard shown = board();
    const auto look = [](std::string id, DownloadState state, std::uint64_t received,
                         std::uint64_t total, std::string detail = {}) {
        std::string name = "Core " + id;
        return RomsetListLook{.id = std::move(id),
                              .name = std::move(name),
                              .state = state,
                              .received = received,
                              .total = total,
                              .detail = std::move(detail)};
    };
    shown.update({.romsetLists = {look("a", DownloadState::Downloading, 25, 100),
                                  look("b", DownloadState::Queued, 0, 0)}});
    const DownloadRow* a = shown.find("romsets:a");
    expect(a != nullptr && a->kind == DownloadKind::RomsetList &&
               a->title == "Romset list · Core a" && a->state == DownloadState::Downloading &&
               a->fraction == 0.25 && a->bytesDone == 25 && a->bytesTotal == 100,
           "a romset list downloading is a row with its bytes");
    expect(opensu::app::actionsFor(*a).empty(), "a romset list has no buttons");
    expect(shown.find("romsets:b")->state == DownloadState::Queued, "a waiting list is queued");
    shown.update({.romsetLists = {look("a", DownloadState::Queued, 0, 0),
                                  look("b", DownloadState::Queued, 0, 0),
                                  look("c", DownloadState::Queued, 0, 0)}});
    shown.update({.romsetLists = {look("c", DownloadState::Queued, 0, 0),
                                  look("b", DownloadState::Queued, 0, 0),
                                  look("a", DownloadState::Queued, 0, 0)}});
    expect(shown.rows()[0].key == "romsets:c" && shown.rows()[2].key == "romsets:a",
           "waiting lists are in the order they are fetched in");
    shown = board();
    shown.update({.romsetLists = {look("a", DownloadState::Downloading, 25, 100),
                                  look("b", DownloadState::Queued, 0, 0)}});

    shown.update({.romsetLists = {look("a", DownloadState::Downloading, 100, 100),
                                  look("b", DownloadState::Downloading, 0, 0)}});
    expect(shown.find("romsets:a")->state == DownloadState::Installing,
           "a list fully received is being read");

    shown.update({.romsetLists = {look("a", DownloadState::Finished, 100, 100),
                                  look("b", DownloadState::Failed, 0, 0, "HTTP 404")}});
    shown.update({.romsetLists = {look("a", DownloadState::Finished, 100, 100),
                                  look("b", DownloadState::Failed, 0, 0, "HTTP 404")}});
    expect(shown.rows().size() == 2 && shown.rows()[0].state == DownloadState::Finished &&
               shown.rows()[1].key == "romsets:b" && shown.rows()[1].detail == "HTTP 404",
           "ended lists stay, a failure with its reason");
}

} // namespace

int main() {
    testMergesEverySource();
    testOrdering();
    testSteamStates();
    testSteamActions();
    testInstallStates();
    testUninstallStates();
    testUpdateStates();
    testSteamInstallJoinsItsQueueRow();
    testSteamUnreadable();
    testRemoved();
    testEmulatorEnds();
    testLabels();
    testRomsetLists();
    std::printf("downloads_board: all checks passed\n");
    return 0;
}
