#include "launcher_status.hpp"

#include <algorithm>

namespace opensu::app {
namespace {

ui::ServiceState steamState(launch::SteamState state) {
    switch (state) {
    case launch::SteamState::Initializing:
        return ui::ServiceState::Starting;
    case launch::SteamState::Ready:
        return ui::ServiceState::Ready;
    case launch::SteamState::Failed:
        return ui::ServiceState::Failed;
    case launch::SteamState::Blocked:
        return ui::ServiceState::Blocked;
    case launch::SteamState::Stopped:
        break;
    }
    return ui::ServiceState::Hidden;
}

ui::ServiceState storeState(library::Source source,
                            std::span<const library::SourceStatus> sources) {
    const auto found = std::ranges::find(sources, source, &library::SourceStatus::source);
    if (found == sources.end()) {
        return ui::ServiceState::Hidden;
    }
    switch (found->availability) {
    case library::Availability::Ready:
        return ui::ServiceState::Ready;
    case library::Availability::Attention:
        return ui::ServiceState::Failed;
    case library::Availability::Absent:
        break;
    }
    return ui::ServiceState::Hidden;
}

std::string_view name(ui::Icon icon) {
    switch (icon) {
    case ui::Icon::Steam:
        return "steam";
    case ui::Icon::Epic:
        return "epic";
    case ui::Icon::Gog:
        return "gog";
    }
    return "unknown";
}

std::string_view name(ui::ServiceState state) {
    switch (state) {
    case ui::ServiceState::Starting:
        return "starting";
    case ui::ServiceState::Ready:
        return "ready";
    case ui::ServiceState::Failed:
        return "failed";
    case ui::ServiceState::Blocked:
        return "blocked";
    case ui::ServiceState::Hidden:
        break;
    }
    return "hidden";
}

} // namespace

std::vector<ui::LauncherBadge> launcherBadges(launch::SteamState steam,
                                              std::span<const steam::Download> downloads,
                                              std::span<const library::SourceStatus> sources) {
    ui::LauncherBadge steamBadge{.icon = ui::Icon::Steam, .state = steamState(steam)};
    const auto active = std::ranges::find_if(downloads, &steam::Download::active);
    if (steamBadge.state == ui::ServiceState::Ready && active != downloads.end()) {
        steamBadge.progress = active->progress;
    }
    return {steamBadge,
            ui::LauncherBadge{.icon = ui::Icon::Epic,
                              .state = storeState(library::Source::Epic, sources)},
            ui::LauncherBadge{.icon = ui::Icon::Gog,
                              .state = storeState(library::Source::Gog, sources)}};
}

std::string describe(std::span<const ui::LauncherBadge> badges) {
    std::string out;
    for (const ui::LauncherBadge& badge : badges) {
        if (badge.state == ui::ServiceState::Hidden) {
            continue;
        }
        if (!out.empty()) {
            out += ' ';
        }
        out.append(name(badge.icon)).append("=").append(name(badge.state));
        if (badge.progress) {
            out += ":" + std::to_string(static_cast<int>(*badge.progress * 100.0)) + "%";
        }
    }
    return out;
}

} // namespace opensu::app
