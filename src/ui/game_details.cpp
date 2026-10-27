#include "game_details.hpp"

#include <algorithm>
#include <ctime>

#include "library/rom_systems.hpp"

namespace opensu::ui {
namespace {

constexpr long long monthSeconds = 86400LL * 31;

std::string plural(long long count, const char* unit) {
    return std::to_string(count) + " " + unit + (count == 1 ? "" : "s") + " ago";
}

std::string badgeOf(const library::Game& game) {
    if (game.source == library::Source::Rom) {
        if (const library::roms::RomSystem* system = library::roms::systemByKey(game.sourceId)) {
            return std::string{system->label};
        }
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

/// The Emulator choice: every emulator of the system, the game's pick chosen, led by
/// `noEmulator` while it has none.
SettingsRow emulatorChoice(const library::Game& game, const std::string& source) {
    Options options;
    for (const library::EmulatorOption& option : game.emulatorOptions) {
        options.labels.push_back(option.name);
    }
    const auto chosen = std::ranges::find(options.labels, game.emulator);
    if (chosen == options.labels.end()) {
        options.labels.insert(options.labels.begin(), std::string{noEmulator});
    } else {
        options.chosen = static_cast<std::size_t>(chosen - options.labels.begin());
    }
    return makeChoice(std::string{detailsEmulatorId}, "Emulator", source, std::move(options));
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
        return plural(seconds / 60, "minute");
    }
    if (seconds < 86400) {
        return plural(seconds / 3600, "hour");
    }
    if (seconds < monthSeconds) {
        return plural(seconds / 86400, "day");
    }
    const std::time_t when = std::chrono::system_clock::to_time_t(*at);
    std::tm parts{};
    localtime_r(&when, &parts);
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

GameDetails detailsOf(const library::Game& game, bool hidden,
                      std::chrono::system_clock::time_point now, const std::string& download,
                      bool ownWinePrefix, const GameSettingsView& settings) {
    GameDetails details;
    details.gameId = game.id;
    details.title = game.title;
    details.badge = badgeOf(game);
    details.art = game.artwork.empty() ? game.artworkWide : game.artwork;
    details.hero = game.artwork.empty() ? std::filesystem::path{} : game.artworkWide;
    const bool rom = game.source == library::Source::Rom;
    const bool own = game.source == library::Source::Own;
    const bool updatable = game.installed && game.updateAvailable && !rom;
    std::vector<SettingsRow>& facts = details.facts;
    facts.push_back(detailsFact(
        "Status", std::string{game.installed ? "Installed" : "Not installed"} +
                      (updatable ? ", update available" : "") + (hidden ? ", hidden" : "")));
    if (game.ownedIn.size() > 1) {
        std::string owned;
        for (const library::Source source : game.ownedIn) {
            owned += (owned.empty() ? "" : ", ") + std::string{library::label(source)};
        }
        facts.push_back(detailsFact("Owned in", owned));
    }
    facts.push_back(detailsFact("Last played", lastPlayedText(game.lastPlayed, now)));
    if (game.playtimeMinutes > 0) {
        facts.push_back(detailsFact("Play time", playtimeText(game.playtimeMinutes)));
    }
    if (own) {
        facts.push_back(game.file.empty() ? detailsFact("Runs through", "Lutris")
                                          : detailsFact("File", game.file.string()));
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

    details.launch = game.installed ? "Play" : "Install";
    std::vector<SettingsRow>& actions = details.actions;
    if (updatable) {
        actions.push_back(action(detailsUpdateId, "Update"));
    }
    if (const library::EmulatorOption* missing = emulatorToDownload(game)) {
        actions.push_back(action(detailsDownloadId, "Download " + missing->name, download));
    }
    if (rom && game.emulatorOptions.size() > 1) {
        actions.push_back(emulatorChoice(game, settings.emulatorSource));
    }
    if (settings.gameSettings) {
        actions.push_back(
            makePage(std::string{detailsSettingsId}, "Game settings", {}, *settings.gameSettings));
    }
    if (game.windowsInstall && !own) {
        actions.push_back(makeChoice(
            std::string{detailsWinePrefixId}, "Wine prefix", {},
            Options{{std::string{sharedPrefix}, std::string{ownPrefix}}, ownWinePrefix ? 1U : 0U}));
    }
    actions.push_back(action(detailsHiddenId, hidden ? "Show game" : "Hide game"));
    if (game.installed && !rom && !own) {
        actions.push_back(action(detailsUninstallId, "Uninstall"));
    }
    if (own) {
        actions.push_back(action(detailsRenameId, "Rename"));
        if (!game.file.empty()) {
            actions.push_back(action(detailsChangeFileId, "Change file"));
        }
        actions.push_back(action(detailsRemoveId, "Remove from library"));
    }
    actions.push_back(makePage(std::string{detailsArtworkId}, "Choose artwork", {}, {}));
    actions.push_back(makePage(std::string{detailsControlsId}, "Controls", {}, {}));
    actions.push_back(action(detailsBackId, "Back"));
    return details;
}

const SettingsRow* factOf(const GameDetails& details, std::string_view label) {
    const auto found = std::ranges::find(details.facts, label, &SettingsRow::label);
    return found == details.facts.end() ? nullptr : &*found;
}

const SettingsRow* actionOf(const GameDetails& details, std::string_view id) {
    const auto found = std::ranges::find(details.actions, id, &SettingsRow::id);
    return found == details.actions.end() ? nullptr : &*found;
}

} // namespace opensu::ui
