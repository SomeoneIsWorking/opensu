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
    case library::Availability::Loading:
        return ui::ServiceState::Starting;
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
    case ui::Icon::Downloads:
        return "downloads";
    case ui::Icon::Linux:
    case ui::Icon::Wine:
    case ui::Icon::AppImage:
    case ui::Icon::Libretro:
    case ui::Icon::Home:
    case ui::Icon::HomeSelected:
    case ui::Icon::Library:
    case ui::Icon::LibrarySelected:
    case ui::Icon::Windows:
    case ui::Icon::MacOs:
    case ui::Icon::Stream:
        break;
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

ui::LauncherBadge downloadsBadge(std::span<const DownloadRow> rows) {
    ui::LauncherBadge badge{.icon = ui::Icon::Downloads,
                            .source = std::nullopt,
                            .state = ui::ServiceState::Hidden,
                            .progress = std::nullopt};
    std::uint64_t done = 0;
    std::uint64_t total = 0;
    double fractions = 0.0;
    int measured = 0;
    for (const DownloadRow& row : rows) {
        // An update on offer is not under way until the player asks for it.
        if (ended(row.state) || row.state == DownloadState::Available) {
            continue;
        }
        badge.state = ui::ServiceState::Ready;
        if (row.bytesTotal > 0) {
            done += std::min(row.bytesDone, row.bytesTotal);
            total += row.bytesTotal;
        }
        if (row.fraction) {
            fractions += std::clamp(*row.fraction, 0.0, 1.0);
            ++measured;
        }
    }
    if (total > 0) {
        badge.progress = static_cast<double>(done) / static_cast<double>(total);
    } else if (measured > 0) {
        badge.progress = fractions / measured;
    } else if (badge.state == ui::ServiceState::Ready) {
        badge.state = ui::ServiceState::Starting;
    }
    return badge;
}

} // namespace

std::vector<ui::LauncherBadge> launcherBadges(launch::SteamState steam,
                                              std::span<const DownloadRow> downloads,
                                              std::span<const library::SourceStatus> sources) {
    return {ui::LauncherBadge{.icon = ui::Icon::Steam,
                              .source = library::Source::Steam,
                              .state = steamState(steam)},
            ui::LauncherBadge{.icon = ui::Icon::Epic,
                              .source = library::Source::Epic,
                              .state = storeState(library::Source::Epic, sources)},
            ui::LauncherBadge{.icon = ui::Icon::Gog,
                              .source = library::Source::Gog,
                              .state = storeState(library::Source::Gog, sources)},
            downloadsBadge(downloads)};
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
