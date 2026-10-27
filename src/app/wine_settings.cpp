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
    // Choice 0 is Automatic, then each runner found.
    ui::Options runners;
    runners.labels.push_back(found_.empty() ? "None found" : "Automatic: " + found_.front().name);
    for (const library::wine::Runner& runner : found_) {
        runners.labels.push_back(runner.name);
    }
    if (!v.wineRunner.empty()) {
        const auto at = std::ranges::find(found_, v.wineRunner, &library::wine::Runner::wine);
        if (at == found_.end()) {
            runners.labels.push_back(runnerName(found_, v.wineRunner));
            runners.chosen = runners.labels.size() - 1;
        } else {
            runners.chosen = static_cast<std::size_t>(at - found_.begin()) + 1;
        }
    }
    page.rows.push_back(ui::makeChoice(runnerId, "Wine runner",
                                       "What Windows GOG and Epic games run under",
                                       std::move(runners)));
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
    // Choice 0 is Automatic, then each runner found, then a chosen runner no longer found.
    fs::path& chosen = preferences_.values().wineRunner;
    const auto at = std::ranges::find(found_, chosen, &library::wine::Runner::wine);
    const bool missing = !chosen.empty() && at == found_.end();
    const int count = static_cast<int>(found_.size()) + (missing ? 2 : 1);
    int from = 0;
    if (missing) {
        from = count - 1;
    } else if (!chosen.empty()) {
        from = static_cast<int>(at - found_.begin()) + 1;
    }
    const int to = (from + (step == 0 ? 1 : step) % count + count) % count;
    if (missing && to == count - 1) {
        return;
    }
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
