#include "wine_settings.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>

#include "library/steam.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using ui::RowKind;

constexpr const char* runnerId = "wine.runner";
constexpr const char* prefixId = "wine.prefix";
constexpr const char* ownId = "wine.own";
constexpr const char* steamId = "wine.steam";

std::string runnerName(const std::vector<library::wine::Runner>& found, const fs::path& wine) {
    const auto at = std::ranges::find(found, wine, &library::wine::Runner::wine);
    return at == found.end() ? wine.string() + " (not found)" : at->name;
}

} // namespace

library::wine::Search WineSettings::search() const {
    library::wine::Search search{
        .executablePath = config_.executablePath, .steamRoots = {}, .steamLibraries = {}};
    if (!integrations_.lists(library::Source::Steam)) {
        return search;
    }
    const library::steam::Library steam =
        library::steam::Library::discover(config_.home, config_.steamRoots);
    search.steamRoots = steam.roots();
    for (const library::steam::LibraryFolder& folder : steam.libraryFolders()) {
        search.steamLibraries.push_back(folder.path);
    }
    return search;
}

void WineSettings::rescan() {
    found_ = library::wine::discover(search());
}

ui::SettingsCategory WineSettings::page() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{std::string{categoryId}, "Wine", {}};
    std::string runner = "None found";
    if (!v.wineRunner.empty()) {
        runner = runnerName(found_, v.wineRunner);
    } else if (!found_.empty()) {
        runner = "Automatic: " + found_.front().name;
    }
    page.rows.push_back(ui::makeRow(runnerId, RowKind::Choice, "Wine runner",
                                    "What Windows GOG and Epic games run under", runner));
    page.rows.push_back(ui::makeRow(prefixId, RowKind::Info, "Shared Wine prefix",
                                    "Every Windows game without its own prefix",
                                    library::wine::sharedPrefix(config_.dataDir).string()));
    const std::size_t own = v.ownWinePrefixes.ids().size();
    page.rows.push_back(
        ui::makeRow(ownId, RowKind::Info, "Own Wine prefixes", "Set per game on its details page",
                    own == 0 ? "None" : std::to_string(own) + (own == 1 ? " game" : " games")));
    if (integrations_.lists(library::Source::Steam)) {
        page.rows.push_back(ui::makeRow(steamId, RowKind::Info, "Steam games",
                                        "Steam runs them with the tool picked in Steam",
                                        "Left to Steam"));
    }
    return page;
}

bool WineSettings::owns(const std::string& id) {
    return id.starts_with("wine.");
}

void WineSettings::run(const std::string& id, int step) {
    if (id != runnerId) {
        return;
    }
    // Choice 0 is Automatic, then each runner found.
    fs::path& chosen = preferences_.values().wineRunner;
    const auto at = std::ranges::find(found_, chosen, &library::wine::Runner::wine);
    const int count = static_cast<int>(found_.size()) + 1;
    const int from =
        chosen.empty() || at == found_.end() ? 0 : static_cast<int>(at - found_.begin()) + 1;
    const int to = (from + (step == 0 ? 1 : step) + count) % count;
    chosen = to == 0 ? fs::path{} : found_[static_cast<std::size_t>(to - 1)].wine;
    preferences_.save();
}

std::string WineSettings::prepare(library::Game& game) const {
    if (!game.windowsInstall) {
        return {};
    }
    const settings::Settings& v = preferences_.values();
    return library::wine::prepare(
        game,
        library::wine::Choice{.runner = v.wineRunner,
                              .found = v.wineRunner.empty() ? library::wine::discover(search())
                                                            : std::vector<library::wine::Runner>{},
                              .dataDir = config_.dataDir,
                              .own = v.ownWinePrefixes});
}

} // namespace opensu::app
