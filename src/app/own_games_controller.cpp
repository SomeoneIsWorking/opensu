#include "own_games_controller.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "library/lutris_games.hpp"
#include "library/lutris_wine.hpp"
#include "library/own_games.hpp"
#include "library/steam.hpp"
#include "library/steam_shortcuts.hpp"
#include "lucent/log.h"
#include "net/web_client.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using library::own::Entry;

constexpr const char* titleCardId = "own-games/title";
/// The most bytes a title holds.
constexpr std::size_t titleMostLength = 256;

std::string countOf(std::size_t count, const char* one, const char* many) {
    return std::to_string(count) + " " + (count == 1 ? one : many);
}

std::string kindName(std::optional<library::own::Kind> kind) {
    if (!kind) {
        return "A file";
    }
    switch (*kind) {
    case library::own::Kind::Windows:
        return "A Windows game, run under Wine";
    case library::own::Kind::AppImage:
        return "A Linux AppImage";
    case library::own::Kind::Script:
        return "A Linux script";
    case library::own::Kind::Native:
        break;
    }
    return "A Linux program";
}

/// The guess in words for the confirm card.
std::string noteOf(const library::identify::Guess& guess) {
    const std::string match = guess.steamAppId.empty()
                                  ? "No store match, so no cover yet"
                                  : "Steam app " + guess.steamAppId + ", its cover from Steam";
    return kindName(guess.kind) + ". " + match + ". Title from " + guess.from + ".";
}

} // namespace

OwnGamesController::Places OwnGamesController::Places::of(const config::Config& config,
                                                          std::filesystem::path records) {
    return Places{.records = std::move(records),
                  .home = config.home,
                  .steamRoots =
                      library::steam::Library::discover(config.home, config.steamRoots).roots(),
                  .lutrisData = library::lutris::dataDir(config.xdgDataHome)};
}

OwnGamesController::OwnGamesController(Parts parts, Places places, Hooks hooks)
    : navigator_{parts.navigator}, paths_{parts.paths}, keyboard_{parts.keyboard},
      sounds_{parts.sounds}, places_{std::move(places)}, hooks_{std::move(hooks)},
      records_{places_.records} {
}

std::vector<Entry> OwnGamesController::notImported(std::vector<Entry> found) const {
    std::vector<Entry> known;
    try {
        known = records_.all();
    } catch (const std::runtime_error& error) {
        lucent::warn("own-games", "{}", error.what());
        return {};
    }
    std::erase_if(found, [&known](const Entry& entry) {
        return std::ranges::any_of(known, [&entry](const Entry& kept) {
            return kept.origin == entry.origin && kept.originId == entry.originId;
        });
    });
    return found;
}

std::vector<Entry> OwnGamesController::steamShortcuts() const {
    return notImported(library::own::steamShortcuts(places_.steamRoots));
}

std::vector<Entry> OwnGamesController::lutrisGames() const {
    try {
        return notImported(library::own::lutrisGames(places_.lutrisData));
    } catch (const std::runtime_error& error) {
        lucent::warn("own-games", "Lutris: {}", error.what());
        return {};
    }
}

void OwnGamesController::openAdd() {
    const std::size_t shortcuts = steamShortcuts().size();
    const std::size_t lutris = lutrisGames().size();
    std::vector<std::string> options{"Choose a file: a program, script, AppImage or Windows .exe"};
    std::vector<std::function<void()>> actions{[this] {
        chooseFile();
    }};
    if (shortcuts > 0) {
        options.push_back("Import " + countOf(shortcuts, "Steam shortcut", "Steam shortcuts"));
        actions.emplace_back([this] {
            importSteamShortcuts();
        });
    }
    if (lutris > 0) {
        options.push_back("Import " + countOf(lutris, "Lutris game", "Lutris games"));
        actions.emplace_back([this] {
            importLutris();
        });
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    // Nothing is chosen yet, so every option acts.
    const std::size_t none = options.size();
    navigator_.push(std::make_unique<gpu2d::ui::ChoiceList>(
        "Add a game", std::move(options), none, [actions = std::move(actions)](std::size_t picked) {
            actions.at(picked)();
        }));
}

void OwnGamesController::chooseFile() {
    std::error_code error;
    const fs::path games = places_.home / "Games";
    pickFile("Add a game", fs::is_directory(games, error) ? games : places_.home,
             [this](const fs::path& file) {
                 identify(file);
             });
}

void OwnGamesController::identify(const fs::path& file) {
    hooks_.say("Looking up " + file.filename().string(), false);
    identifier_ = std::jthread{[this, file = file] {
        const net::WebClient web;
        library::identify::Guess guess =
            library::identify::identify(file, [&web](const std::string& term) {
                std::string error;
                const std::optional<std::string> answer =
                    web.get(library::identify::storeSearchUrl(term), error);
                if (!answer) {
                    lucent::warn("own-games", "Steam store search: {}", error);
                    return std::vector<library::identify::StoreApp>{};
                }
                return library::identify::parseStoreSearch(*answer);
            });
        lucent::info("own-games", "{} is {} (Steam app {}), from {}", file.string(), guess.title,
                     guess.steamAppId.empty() ? "none" : guess.steamAppId, guess.from);
        const std::lock_guard lock{mutex_};
        identified_.emplace(file, std::move(guess));
    }};
}

void OwnGamesController::service() {
    std::optional<std::pair<fs::path, library::identify::Guess>> ready;
    {
        const std::lock_guard lock{mutex_};
        ready = std::exchange(identified_, std::nullopt);
    }
    if (ready) {
        confirm(ready->first, ready->second);
    }
}

void OwnGamesController::confirm(const fs::path& file, const library::identify::Guess& guess) {
    askTitle("Add " + file.filename().string() + "?", noteOf(guess), guess.title,
             [this, file = file, steamAppId = guess.steamAppId](const std::string& title) {
                 apply(
                     [this, &file, &title, &steamAppId] {
                         records_.add(Entry{
                             .id = {}, .title = title, .file = file, .steamAppId = steamAppId});
                     },
                     "Added " + title);
             });
}

void OwnGamesController::pickFile(const std::string& title, const fs::path& start,
                                  std::function<void(const fs::path&)> picked) {
    paths_.begin(
        PathEditor::Request{.title = title,
                            .start = start,
                            .clearLabel = {},
                            .refusal = &library::own::refusal,
                            .files =
                                [](const fs::path& file) {
                                    return library::own::kindOf(file).has_value();
                                },
                            .done =
                                [picked = std::move(picked)](const std::optional<fs::path>& file) {
                                    if (file) {
                                        picked(*file);
                                    }
                                }});
}

void OwnGamesController::importAll(std::vector<Entry> entries, std::string_view from) {
    const std::size_t count = entries.size();
    apply(
        [this, &entries] {
            for (Entry& entry : entries) {
                records_.add(std::move(entry));
            }
        },
        "Imported " + countOf(count, "game", "games") + " from " + std::string{from});
}

void OwnGamesController::importSteamShortcuts() {
    importAll(steamShortcuts(), "Steam's shortcuts");
}

void OwnGamesController::importLutris() {
    importAll(lutrisGames(), "Lutris");
}

void OwnGamesController::rename(const library::Game& game) {
    askTitle("Rename", "The name the library shows.", game.title,
             [this, id = game.sourceId](const std::string& title) {
                 apply(
                     [this, &id, &title] {
                         records_.rename(id, title);
                     },
                     "Renamed to " + title);
             });
}

void OwnGamesController::changeFile(const library::Game& game) {
    pickFile("Change " + game.title + "'s file",
             game.file.empty() ? places_.home : game.file.parent_path(),
             [this, id = game.sourceId, title = game.title](const fs::path& file) {
                 apply(
                     [this, &id, &file] {
                         records_.setFile(id, file);
                     },
                     title + " now starts " + file.filename().string());
             });
}

void OwnGamesController::remove(const library::Game& game) {
    const std::string where = game.file.empty() ? std::string{"Lutris keeps it as it is."}
                                                : game.file.string() + " stays where it is.";
    navigator_.push(gpu2d::ui::confirm("Remove " + game.title + " from the library?",
                                       "Only openSU's entry goes. " + where, "Remove",
                                       [this, id = game.sourceId, title = game.title] {
                                           apply(
                                               [this, &id] {
                                                   records_.remove(id);
                                               },
                                               "Removed " + title + " from the library");
                                       }));
}

void OwnGamesController::askTitle(const std::string& heading, const std::string& note,
                                  std::string title, std::function<void(const std::string&)> done) {
    title_ = std::move(title);
    titled_ = std::move(done);
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = titleCardId,
                  .title = heading,
                  .note = note,
                  .label = "Title",
                  .placeholder = "Title",
                  .secret = false,
                  .mostBytes = titleMostLength,
                  .text = [this]() -> const std::string& {
                      return title_;
                  },
                  .changed =
                      [this](std::string text) {
                          title_ = std::move(text);
                      },
                  .submitted =
                      [this] {
                          if (title_.empty()) {
                              hooks_.say("A game needs a title", true);
                              return;
                          }
                          closeTitle();
                          if (titled_) {
                              std::exchange(titled_, {})(title_);
                          }
                      },
                  .cancelled =
                      [this] {
                          closeTitle();
                          titled_ = {};
                      }}));
    keyboard_.open();
}

void OwnGamesController::closeTitle() {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == titleCardId) {
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

void OwnGamesController::apply(const std::function<void()>& change, const std::string& done) {
    try {
        change();
    } catch (const std::runtime_error& error) {
        hooks_.say(error.what(), true);
        return;
    }
    hooks_.changed();
    hooks_.say(done, false);
}

} // namespace opensu::app
