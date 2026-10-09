// breadcrumb_trail — the one place the breadcrumb trail is computed from where the shell is: the
// section, the folder open in Library, a search, the filters narrowing the view, an open
// game's details page, the Settings screen's category and the Devices page's tab.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "library/library_query.hpp"
#include "library/sections.hpp"
#include "library/shelf.hpp"
#include "ui/breadcrumbs.hpp"

namespace opensu::app {

/// Where the shell is.
struct TrailState {
    library::Section section{library::Section::Home};
    /// The folder open inside Library.
    std::optional<library::Folder> folder;
    /// The search whose results are showing; empty for none.
    std::string search;
    /// What narrows the view, as the player reads it ("Installed").
    std::vector<std::string> filters;
    /// The title of the game whose details page is open; empty for none.
    std::string game;
    /// The category on show while the Settings screen is open; empty when it is not.
    std::string settings;
    /// The tab on show while the Devices page is open; empty when it is not.
    std::string devices;
};

/// The trail for `state`, first level first: the section, its folder, the search, the filters, the
/// game. The Settings screen and the Devices page replace them all: "Settings" or "Devices", then
/// the category or tab.
[[nodiscard]] ui::Trail trailOf(const TrailState& state);

/// The filters of `options` as the player reads them. `sources` names each source filter.
[[nodiscard]] std::vector<std::string> filtersOf(const library::ViewOptions& options,
                                                 const std::vector<library::SourceChoice>& sources);

} // namespace opensu::app
