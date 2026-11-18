// The top bar's badges from Steam's client state, the catalog's store statuses and the Downloads
// page's rows.
#include "launcher_status.hpp"
#include "support/test_main.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using opensu::app::DownloadKind;
using opensu::app::DownloadRow;
using opensu::app::DownloadState;
using opensu::launch::SteamState;
using opensu::library::Availability;
using opensu::library::Source;
using opensu::library::SourceStatus;
using opensu::ui::LauncherBadge;
using opensu::ui::ServiceState;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

std::vector<SourceStatus> stores() {
    return std::vector<SourceStatus>{
        SourceStatus{.source = Source::Steam, .availability = Availability::Ready, .detail = {}},
        SourceStatus{.source = Source::Epic,
                     .availability = Availability::Attention,
                     .detail = "legendary is not logged in"},
        SourceStatus{.source = Source::Gog, .availability = Availability::Absent, .detail = {}},
    };
}

void testStoresAndStartingSteam() {
    const std::vector<LauncherBadge> badges =
        opensu::app::launcherBadges(SteamState::Initializing, {}, stores());
    expect(badges.size() == 4 && badges[3].state == ServiceState::Hidden,
           "one badge per launcher, and Downloads hidden while nothing downloads");
    expect(badges[0].state == ServiceState::Starting && !badges[0].progress,
           "Steam signing in is starting");
    expect(badges[1].state == ServiceState::Failed, "Epic signed out needs the player");
    expect(badges[2].state == ServiceState::Hidden, "GOG not installed shows no badge");
    expect(opensu::app::describe(badges) == "steam=starting epic=failed",
           "the channel names the visible badges");
}

DownloadRow row(std::string key, DownloadState state, std::optional<double> fraction,
                std::uint64_t done, std::uint64_t total) {
    return DownloadRow{.key = std::move(key),
                       .kind = DownloadKind::Emulator,
                       .state = state,
                       .fraction = fraction,
                       .bytesDone = done,
                       .bytesTotal = total};
}

void testDownloadsBadge() {
    const std::vector<LauncherBadge> idle =
        opensu::app::launcherBadges(SteamState::Ready, {}, stores());
    expect(idle.size() == 4 && idle[3].state == ServiceState::Hidden && !idle[3].source,
           "nothing downloading, no Downloads badge");
    const std::vector<DownloadRow> offered{
        row("update:a", DownloadState::Available, std::nullopt, 0, 0)};
    expect(opensu::app::launcherBadges(SteamState::Ready, offered, stores())[3].state ==
               ServiceState::Hidden,
           "an update only on offer shows no badge");
    const std::vector<DownloadRow> rows{
        row("romsets:a", DownloadState::Downloading, 0.5, 30, 100),
        row("emulator:b", DownloadState::Queued, std::nullopt, 0, 300),
        row("emulator:c", DownloadState::Failed, std::nullopt, 0, 900)};
    const std::vector<LauncherBadge> busy =
        opensu::app::launcherBadges(SteamState::Ready, rows, stores());
    expect(busy[3].state == ServiceState::Ready && busy[3].progress == 30.0 / 400.0,
           "the live rows' bytes ring the Downloads badge; an ended row does not count");
    expect(busy[0].state == ServiceState::Ready && !busy[0].progress,
           "the Steam badge shows Steam's state only");
    expect(opensu::app::describe(busy).ends_with("downloads=ready:7%"), "progress is published");
    const std::vector<DownloadRow> fractions{
        row("update:a", DownloadState::Installing, 0.2, 0, 0),
        row("update:b", DownloadState::Downloading, 0.6, 0, 0)};
    expect(opensu::app::launcherBadges(SteamState::Ready, fractions, stores())[3].progress == 0.4,
           "rows without bytes ring it with their mean fraction");
    const std::vector<DownloadRow> unmeasured{
        row("romsets:a", DownloadState::Queued, std::nullopt, 0, 0)};
    const LauncherBadge spinning =
        opensu::app::launcherBadges(SteamState::Ready, unmeasured, stores())[3];
    expect(spinning.state == ServiceState::Starting && !spinning.progress,
           "a download nothing measures spins");
    expect(opensu::app::launcherBadges(SteamState::Stopped, {}, {})[0].state ==
               ServiceState::Hidden,
           "no Steam, no badge");
}

} // namespace

int main() try {
    testStoresAndStartingSteam();
    testDownloadsBadge();
    std::printf("launcher_status: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
