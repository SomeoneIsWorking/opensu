#include "game_details.hpp"

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <iterator>
#include <span>
#include <utility>

#include "clock/local_time.hpp"
#include "library/match_pick.hpp"
#include "library/rom_systems.hpp"
#include "lucent/text.h"

namespace opensu::ui {
namespace {

constexpr long long monthSeconds = 86400LL * 31;

std::string ago(long long count, const char* unit) {
    return lucent::text::counted(static_cast<std::size_t>(count), unit) + " ago";
}

std::string badgeOf(const library::Game& game) {
    if (game.source == library::Source::Rom) {
        if (const library::roms::RomSystem* system = library::roms::systemByKey(game.sourceId)) {
            return std::string{system->label};
        }
    }
    if (game.source == library::Source::Remote) {
        return "On " + game.hostName;
    }
    return std::string{library::label(game.source)};
}

std::string playtimeText(int minutes) {
    if (minutes >= 60) {
        return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
    }
    return std::to_string(minutes) + " min";
}

/// The ROM's emulator as a fact: its name and whether it is installed, or what to install.
std::string emulatorText(const library::Game& game) {
    if (game.emulatorOptions.empty()) {
        return "none known for this system";
    }
    const auto chosen =
        std::ranges::find(game.emulatorOptions, game.emulator, &library::EmulatorOption::name);
    if (chosen == game.emulatorOptions.end()) {
        if (std::ranges::any_of(game.emulatorOptions, [](const library::EmulatorOption& option) {
                return option.romset == library::RomsetFit::Pending;
            })) {
            return "picked once the romset is checked";
        }
        if (!game.romset.empty()) {
            return "none for this romset";
        }
        return game.unavailable.empty() ? "none installed" : game.unavailable;
    }
    std::string text = chosen->name + (chosen->installed ? " · installed" : " · not installed");
    if (chosen->romset == library::RomsetFit::Matches) {
        text += " · romset matches";
    } else if (chosen->romset == library::RomsetFit::Mismatch) {
        text += " · romset does not match";
    }
    return text;
}

SettingsRow action(std::string_view id, std::string label, std::string value = {}) {
    return makeRow(std::string{id}, RowKind::Action, std::move(label), {}, std::move(value));
}

} // namespace

std::string lastPlayedText(const std::optional<std::chrono::system_clock::time_point>& at,
                           std::chrono::system_clock::time_point now) {
    if (!at) {
        return "Never played";
    }
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - *at).count();
    if (seconds < 60) {
        return "Just now";
    }
    if (seconds < 3600) {
        return ago(seconds / 60, "minute");
    }
    if (seconds < 86400) {
        return ago(seconds / 3600, "hour");
    }
    if (seconds < monthSeconds) {
        return ago(seconds / 86400, "day");
    }
    const std::time_t when = std::chrono::system_clock::to_time_t(*at);
    const std::tm parts = clock::localTime(when);
    std::string date(16, '\0');
    date.resize(std::strftime(date.data(), date.size(), "%Y-%m-%d", &parts));
    return date;
}

const library::EmulatorOption* emulatorToDownload(const library::Game& game) {
    if (game.source != library::Source::Rom || !game.launch.empty()) {
        return nullptr;
    }
    const auto missing = [](const library::EmulatorOption& option) {
        return !option.installed && option.downloadable && option.problem.empty();
    };
    const auto& options = game.emulatorOptions;
    if (const auto chosen =
            std::ranges::find(options, game.emulator, &library::EmulatorOption::name);
        chosen != options.end()) {
        return missing(*chosen) ? &*chosen : nullptr;
    }
    // Unpicked, a core the romset does not match, or is not checked against yet, is not offered.
    const auto first = std::ranges::find_if(options, [&missing](const auto& option) {
        return missing(option) && option.romset != library::RomsetFit::Mismatch &&
               option.romset != library::RomsetFit::Pending;
    });
    return first == options.end() ? nullptr : &*first;
}

SettingsRow detailsFact(std::string label, std::string value) {
    std::string id = "fact/" + label;
    return makeRow(std::move(id), RowKind::Info, std::move(label), {}, std::move(value));
}

namespace {

/// A game on the page, whose rows say which copy they act on when the page has several.
struct PageCopy {
    const library::Game& game;
    const GameSettingsView& settings;
    bool ownWinePrefix;
    bool named;

    [[nodiscard]] std::string store() const {
        return std::string{library::label(game.source)};
    }
    /// `row` as this copy's row on the page.
    [[nodiscard]] SettingsRow asNamed(SettingsRow row) const {
        if (named) {
            row.id = copyRowId(game.id, row.id);
            row.label += " (" + store() + ")";
        }
        return row;
    }
};

/// A game the player added themselves, which they can rename and remove.
bool ownedOutright(const library::Game& game) {
    return game.source == library::Source::Own && !game.fromGameFolder;
}

LaunchButton launchOf(const PageCopy& copy) {
    const std::string verb = !copy.game.installed      ? "Install"
                             : copy.settings.continues ? "Continue here"
                                                       : "Play";
    return LaunchButton{copy.named ? verb + " (" + copy.store() + ")" : verb, copy.game.id};
}

std::vector<SettingsRow> factsOf(const library::Game& game, const GameMarks& marks,
                                 std::chrono::system_clock::time_point now,
                                 const GameSettingsView& settings) {
    const bool rom = game.source == library::Source::Rom;
    const bool own = game.source == library::Source::Own;
    const bool updatable = game.installed && game.updateAvailable && !rom;
    std::vector<SettingsRow> facts;
    facts.push_back(detailsFact(
        "Status", std::string{game.installed ? "Installed" : "Not installed"} +
                      (updatable ? ", update available" : "") + (marks.hidden ? ", hidden" : "")));
    facts.push_back(detailsFact("Last played", lastPlayedText(game.lastPlayed, now)));
    if (game.playtimeMinutes > 0) {
        facts.push_back(detailsFact("Play time", playtimeText(game.playtimeMinutes)));
    }
    if (own) {
        facts.push_back(game.file.empty() ? detailsFact("Runs through", "Lutris")
                                          : detailsFact("File", game.file.string()));
        facts.push_back(
            detailsFact("Steam app", game.artworkKey.empty() ? "None matched" : game.artworkKey));
    }
    if (rom) {
        facts.push_back(
            detailsFact("Emulator", emulatorText(game) + (settings.emulatorSource.empty()
                                                              ? std::string{}
                                                              : " · " + settings.emulatorSource)));
        facts.insert(facts.end(), settings.facts.begin(), settings.facts.end());
        if (!game.romset.empty()) {
            facts.push_back(detailsFact("Romset", game.romset));
        }
        if (!game.romsetNearest.empty()) {
            facts.push_back(detailsFact("Nearest", game.romsetNearest));
        }
    }
    if (!settings.sharedSaves.empty()) {
        facts.push_back(detailsFact("Saves", settings.sharedSaves));
    }
    if (!settings.resume.empty()) {
        facts.push_back(detailsFact("Resume", settings.resume));
    }
    return facts;
}

/// The facts: one each where the copies read alike, else one per copy, "Last played (Epic)".
std::vector<SettingsRow> factsOf(const std::vector<PageCopy>& copies, const GameMarks& marks,
                                 std::chrono::system_clock::time_point now) {
    std::vector<std::vector<SettingsRow>> each;
    each.reserve(copies.size());
    for (const PageCopy& copy : copies) {
        each.push_back(factsOf(copy.game, marks, now, copy.settings));
    }
    std::vector<std::string> labels;
    for (const std::vector<SettingsRow>& own : each) {
        for (const SettingsRow& fact : own) {
            if (std::ranges::find(labels, fact.label) == labels.end()) {
                labels.push_back(fact.label);
            }
        }
    }
    std::vector<SettingsRow> facts;
    for (const std::string& label : labels) {
        const auto of = [&label](const std::vector<SettingsRow>& own) {
            return std::ranges::find(own, label, &SettingsRow::label);
        };
        const auto first = of(each.front());
        const bool alike = first != each.front().end() &&
                           std::ranges::all_of(each, [&](const std::vector<SettingsRow>& own) {
                               const auto fact = of(own);
                               return fact != own.end() && fact->value == first->value;
                           });
        if (alike) {
            facts.push_back(*first);
            continue;
        }
        for (std::size_t at = 0; at < each.size(); ++at) {
            if (const auto fact = of(each[at]); fact != each[at].end()) {
                facts.push_back(detailsFact(label + " (" + copies[at].store() + ")", fact->value));
            }
        }
    }
    return facts;
}

std::vector<SettingsRow> updateRows(const library::Game& game) {
    if (game.installed && game.updateAvailable && game.source != library::Source::Rom) {
        return {action(detailsUpdateId, "Update")};
    }
    return {};
}

/// The other ways to play it.
std::vector<SettingsRow> playRows(const PageCopy& copy) {
    const GameSettingsView& settings = copy.settings;
    std::vector<SettingsRow> rows;
    if (!settings.resume.empty() && copy.game.installed) {
        SettingsRow fresh = action(detailsStartFreshId, "Start fresh");
        fresh.confirm = "The game starts from the beginning; where Save and close left it is lost.";
        fresh.confirmVerb = "Start";
        rows.push_back(std::move(fresh));
    }
    if (settings.linked) {
        rows.push_back(action(detailsLinkedId, "Play linked (2 players)"));
    }
    for (const GameSettingsView::Partner& partner : settings.partners) {
        rows.push_back(
            action(std::string{detailsPlayWithPrefix} + partner.id, "Play with " + partner.name));
    }
    if (settings.online) {
        rows.push_back(action(detailsPlayOnlineId, "Play online"));
    }
    return rows;
}

/// What differs by copy, each row naming it.
std::vector<SettingsRow> copyRows(const PageCopy& copy) {
    const library::Game& game = copy.game;
    std::vector<SettingsRow> rows;
    if (game.windowsInstall && game.source != library::Source::Own) {
        rows.push_back(makeChoice(std::string{detailsWinePrefixId}, "Wine prefix", {},
                                  Options{{std::string{sharedPrefix}, std::string{ownPrefix}},
                                          copy.ownWinePrefix ? 1U : 0U}));
    }
    if (!copy.settings.copySaves.empty()) {
        rows.push_back(
            makePage(std::string{detailsSavesId}, "Copy saves", {}, copy.settings.copySaves));
    }
    if (!copy.settings.syncSaves.empty()) {
        rows.push_back(action(detailsSyncSavesId, copy.settings.syncSaves));
    }
    for (SettingsRow& row : rows) {
        row.id = copyRowId(game.id, row.id);
    }
    return rows;
}

/// What the Game settings page holds besides its settings: the title's rows, then what differs by
/// copy, under a heading per store while that copy is installed or on its way.
std::vector<SettingsRow> settingsPageRows(const std::vector<PageCopy>& page) {
    const PageCopy& top = page.front();
    const library::Game& game = top.game;
    std::vector<SettingsRow> rows;
    const auto matchable = std::ranges::find_if(page, [](const PageCopy& copy) {
        return library::matchKindOf(copy.game) != library::MatchKind::None;
    });
    if (matchable != page.end()) {
        rows.push_back(makePage(copyRowId(matchable->game.id, detailsMatchId),
                                "Match a different game", {}, {}));
    }
    if (game.source != library::Source::Remote) {
        rows.push_back(makePage(copyRowId(game.id, detailsArtworkId), "Choose artwork", {}, {}));
        rows.push_back(makePage(copyRowId(game.id, detailsControlsId), "Controls", {}, {}));
    }
    if (!top.named) {
        std::ranges::move(copyRows(top), std::back_inserter(rows));
    }
    if (ownedOutright(game)) {
        rows.push_back(action(copyRowId(game.id, detailsRenameId), "Rename"));
        if (!game.file.empty()) {
            rows.push_back(action(copyRowId(game.id, detailsChangeFileId), "Change file"));
        }
    }
    for (const PageCopy& copy : page) {
        std::vector<SettingsRow> own =
            copy.named && (copy.game.installed || copy.settings.installing)
                ? copyRows(copy)
                : std::vector<SettingsRow>{};
        if (!own.empty()) {
            rows.push_back(makeHeading(copy.store()));
            std::ranges::move(own, std::back_inserter(rows));
        }
    }
    return rows;
}

std::vector<SettingsRow> settingsRows(const std::vector<PageCopy>& page) {
    const PageCopy& top = page.front();
    if (!top.settings.gameSettings && settingsPageRows(page).empty()) {
        return {};
    }
    return {makePage(std::string{detailsSettingsId}, "Game settings", {},
                     top.settings.gameSettings.value_or(std::string{}))};
}

/// What takes the game off this machine or out of the library.
std::vector<SettingsRow> uninstallRows(const library::Game& game) {
    if (ownedOutright(game)) {
        return {action(detailsRemoveId, "Remove from library")};
    }
    const library::Source source = game.source;
    if (game.installed && source != library::Source::Rom && source != library::Source::Own &&
        source != library::Source::Remote) {
        return {action(detailsUninstallId, "Uninstall")};
    }
    return {};
}

} // namespace

std::string copyRowId(std::string_view gameId, std::string_view id) {
    return std::string{copyRowPrefix} + std::string{gameId} + "#" + std::string{id};
}

CopyRow copyRowOf(const std::string& id) {
    const std::size_t mark = id.rfind('#');
    if (!id.starts_with(copyRowPrefix) || mark == std::string::npos) {
        return {id, {}};
    }
    return {id.substr(mark + 1), id.substr(copyRowPrefix.size(), mark - copyRowPrefix.size())};
}

GameDetails detailsOf(const library::Game& game, const GameMarks& marks,
                      std::chrono::system_clock::time_point now, const std::string& download,
                      bool ownWinePrefix, const GameSettingsView& settings,
                      const std::vector<CopyPart>& copies) {
    std::vector<PageCopy> page;
    page.reserve(std::max<std::size_t>(copies.size(), 1));
    for (const CopyPart& copy : copies) {
        page.push_back(PageCopy{copy.game, copy.settings, copy.ownWinePrefix, true});
    }
    if (page.empty()) {
        page.push_back(PageCopy{game, settings, ownWinePrefix, false});
    }
    GameDetails details;
    details.gameId = game.id;
    details.title = game.title;
    details.badge = badgeOf(game);
    details.art = game.artwork.empty() ? game.artworkWide : game.artwork;
    details.hero = game.artwork.empty() ? std::filesystem::path{} : game.artworkWide;
    const PageCopy& top = page.front();
    const std::span<const PageCopy> others = std::span{page}.subspan(1);
    details.launch = launchOf(top);
    details.facts = factsOf(page, marks, now);
    const auto add = [](std::vector<SettingsRow>& to, const PageCopy& copy,
                        std::vector<SettingsRow> rows) {
        for (SettingsRow& row : rows) {
            to.push_back(copy.asNamed(std::move(row)));
        }
    };
    std::vector<SettingsRow>& actions = details.actions;
    add(actions, top, playRows(top));
    add(actions, top, updateRows(top.game));
    if (const library::EmulatorOption* missing = emulatorToDownload(top.game)) {
        actions.push_back(action(detailsDownloadId, "Download " + missing->name, download));
    }
    actions.push_back(action(detailsHomeId, marks.onHome ? "Remove from Home" : "Add to Home"));
    actions.push_back(action(detailsHiddenId, marks.hidden ? "Show game" : "Hide game"));
    std::ranges::move(settingsRows(page), std::back_inserter(actions));
    std::vector<SettingsRow>& more = details.more;
    for (const PageCopy& copy : others) {
        if (copy.game.installed) {
            add(more, copy, {action(detailsLaunchId, "Play")});
        }
    }
    for (const PageCopy& copy : others) {
        add(more, copy, updateRows(copy.game));
    }
    for (const PageCopy& copy : others) {
        if (!copy.game.installed) {
            add(more, copy, {action(detailsInstallId, "Install")});
        }
    }
    for (const PageCopy& copy : page) {
        add(more, copy, uninstallRows(copy.game));
    }
    details.settingsRows = settingsPageRows(page);
    if (!more.empty()) {
        actions.push_back(makePage(std::string{detailsMoreId}, "More", {}, {}));
    }
    actions.push_back(action(detailsBackId, "Back"));
    return details;
}

const SettingsRow* factOf(const GameDetails& details, std::string_view label) {
    const auto found = std::ranges::find(details.facts, label, &SettingsRow::label);
    return found == details.facts.end() ? nullptr : &*found;
}

const SettingsRow* settingsRowOf(const GameDetails& details, std::string_view gameId,
                                 std::string_view id) {
    const auto found =
        std::ranges::find(details.settingsRows, copyRowId(gameId, id), &SettingsRow::id);
    return found == details.settingsRows.end() ? nullptr : &*found;
}

const SettingsRow* actionOf(const GameDetails& details, std::string_view id) {
    const auto found = std::ranges::find(details.actions, id, &SettingsRow::id);
    return found == details.actions.end() ? nullptr : &*found;
}

} // namespace opensu::ui
