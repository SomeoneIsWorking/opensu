#include "breadcrumb_trail.hpp"

#include <algorithm>
#include <string_view>

namespace opensu::app {
namespace {

std::string sectionLabel(library::Section section) {
    return section == library::Section::Home ? "Home" : "Library";
}

} // namespace

ui::Trail trailOf(const TrailState& state) {
    ui::Trail trail;
    if (state.downloads) {
        trail.push_back(ui::Crumb{"Downloads", ui::CrumbKind::Page});
        return trail;
    }
    if (!state.devices.empty()) {
        trail.push_back(ui::Crumb{"Devices", ui::CrumbKind::Devices});
        trail.push_back(ui::Crumb{state.devices, ui::CrumbKind::Page});
        return trail;
    }
    if (!state.settings.empty()) {
        trail.push_back(ui::Crumb{"Settings", ui::CrumbKind::Settings});
        // A page open over a category is a level of its own.
        constexpr std::string_view between = " › ";
        std::string_view rest = state.settings;
        while (!rest.empty()) {
            const std::size_t end = rest.find(between);
            trail.push_back(ui::Crumb{std::string{rest.substr(0, end)}, ui::CrumbKind::Page});
            rest = end == std::string_view::npos ? std::string_view{}
                                                 : rest.substr(end + between.size());
        }
        return trail;
    }
    trail.push_back(ui::Crumb{sectionLabel(state.section), ui::CrumbKind::Section, state.section});
    if (state.folder) {
        trail.push_back(ui::Crumb{library::name(*state.folder), ui::CrumbKind::Folder});
    }
    if (!state.search.empty()) {
        trail.push_back(ui::Crumb{"Search \"" + state.search + "\"", ui::CrumbKind::Search});
    }
    for (const std::string& filter : state.filters) {
        trail.push_back(ui::Crumb{filter, ui::CrumbKind::Filter});
    }
    if (!state.game.empty()) {
        trail.push_back(ui::Crumb{state.game, ui::CrumbKind::Game});
    }
    return trail;
}

std::vector<std::string> filtersOf(const library::ViewOptions& options,
                                   const std::vector<library::SourceChoice>& sources) {
    std::vector<std::string> filters;
    if (!options.source.empty()) {
        const auto named = std::ranges::find(sources, options.source, &library::SourceChoice::key);
        filters.push_back(named == sources.end() ? options.source : named->label);
    }
    if (options.installedOnly) {
        filters.emplace_back("Installed");
    }
    if (options.hiddenOnly) {
        filters.emplace_back("Hidden");
    }
    return filters;
}

} // namespace opensu::app
