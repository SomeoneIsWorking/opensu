// The launcher badges from Steam's client state, its downloads and the catalog's store statuses.
#include "launcher_status.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using opensu::launch::SteamState;
using opensu::library::Availability;
using opensu::library::Source;
using opensu::library::SourceStatus;
using opensu::steam::Download;
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
    expect(badges.size() == 3, "one badge per launcher");
    expect(badges[0].state == ServiceState::Starting && !badges[0].progress,
           "Steam signing in is starting");
    expect(badges[1].state == ServiceState::Failed, "Epic signed out needs the player");
    expect(badges[2].state == ServiceState::Hidden, "GOG not installed shows no badge");
    expect(opensu::app::describe(badges) == "steam=starting epic=failed",
           "the channel names the visible badges");
}

void testDownloadRing() {
    const std::vector<Download> downloads{Download{.appId = "1",
                                                   .progress = 0.2,
                                                   .active = false,
                                                   .paused = true,
                                                   .installing = false,
                                                   .secondsLeft = {}},
                                          Download{.appId = "2",
                                                   .progress = 0.53,
                                                   .active = true,
                                                   .paused = false,
                                                   .installing = false,
                                                   .secondsLeft = {}}};
    const std::vector<LauncherBadge> ready =
        opensu::app::launcherBadges(SteamState::Ready, downloads, stores());
    expect(ready[0].state == ServiceState::Ready && ready[0].progress == 0.53,
           "the active download's progress rings the Steam badge");
    expect(opensu::app::describe(ready).starts_with("steam=ready:53%"), "progress is published");
    const std::vector<LauncherBadge> blocked =
        opensu::app::launcherBadges(SteamState::Blocked, downloads, stores());
    expect(blocked[0].state == ServiceState::Blocked && !blocked[0].progress,
           "a Steam opensu does not own shows no progress");
    expect(opensu::app::launcherBadges(SteamState::Stopped, {}, {})[0].state ==
               ServiceState::Hidden,
           "no Steam, no badge");
}

} // namespace

int main() {
    testStoresAndStartingSteam();
    testDownloadRing();
    std::printf("launcher_status: all checks passed\n");
    return 0;
}
