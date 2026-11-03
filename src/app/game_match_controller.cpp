#include "game_match_controller.hpp"

#include <algorithm>
#include <memory>
#include <system_error>
#include <utility>

#include "fileio/atomic_write.hpp"
#include "game_match_screen.hpp"
#include "library/game_identify.hpp"
#include "library/game_matches.hpp"
#include "lucent/log.h"
#include "net/web_client.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;

/// The most apps one search offers.
constexpr std::size_t mostCandidates = 8;
constexpr const char* nameCardId = "match/name";
constexpr std::size_t nameMostLength = 256;

} // namespace

GameMatchController::GameMatchController(Parts parts, Places places, Hooks hooks)
    : navigator_{parts.navigator}, chrome_{parts.chrome}, images_{parts.images},
      keyboard_{parts.keyboard}, sounds_{parts.sounds}, places_{std::move(places)},
      hooks_{std::move(hooks)} {
}

GameMatchController::~GameMatchController() = default;

const gpu2d::ui::Screen* GameMatchController::screen() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* shown) {
        return dynamic_cast<const GameMatchScreen*>(shown) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool GameMatchController::isOpen() const {
    return screen() != nullptr;
}

void GameMatchController::open(const library::Game& game) {
    if (isOpen()) {
        return;
    }
    game_ = game;
    title_ = "Match a different game: " + game.title;
    sounds_.play(audio::Effect::OpenContextMenu);
    search(game.title);
    navigator_.push(std::make_unique<GameMatchScreen>(*this, chrome_, images_));
}

void GameMatchController::close() {
    const gpu2d::ui::Screen* shown = screen();
    if (shown == nullptr) {
        return;
    }
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(*shown);
}

void GameMatchController::search(const std::string& term) {
    term_ = term;
    candidates_.clear();
    searched_ = false;
    error_.clear();
    const std::uint64_t generation = ++generation_;
    finder_ = std::jthread{[this, term, generation](const std::stop_token& stop) {
        find(stop, term, generation);
    }};
}

void GameMatchController::publish(const Found& found) {
    const std::lock_guard lock{mutex_};
    found_ = found;
}

fs::path GameMatchController::coverOf(const std::string& appId) const {
    fs::path file = places_.covers / (appId + ".jpg");
    std::error_code ec;
    if (fs::is_regular_file(file, ec)) {
        return file;
    }
    const net::WebClient web;
    const std::string base = places_.steamCdn + "/" + appId + "/";
    // The library portrait, else the store header, which every app has.
    for (const char* name : {"library_600x900.jpg", "header.jpg"}) {
        std::string error;
        const std::optional<std::string> bytes = web.get(base + name, error);
        if (!bytes) {
            continue;
        }
        if (!fileio::writeWhole(file, *bytes, error)) {
            lucent::warn("own-games", "{}", error);
            return {};
        }
        return file;
    }
    return {};
}

void GameMatchController::find(const std::stop_token& stop, const std::string& term,
                               std::uint64_t generation) {
    const net::WebClient web;
    Found found{.generation = generation};
    std::vector<library::identify::StoreApp> apps;
    // Steam's search wants every word, so a title with a word Steam's name lacks ("Remake")
    // is searched again without its last words.
    for (std::string asked = term; !asked.empty() && apps.empty();) {
        std::string error;
        const std::optional<std::string> answer =
            web.get(library::identify::storeSearchUrl(asked), error);
        if (!answer) {
            lucent::warn("own-games", "Steam store search: {}", error);
            found.searched = true;
            found.error = "Steam's store cannot be reached";
            publish(found);
            return;
        }
        apps = library::identify::parseStoreSearch(*answer);
        found.term = asked;
        const std::size_t space = asked.find_last_of(' ');
        asked.resize(space == std::string::npos ? 0 : space);
    }
    found.searched = true;
    for (library::identify::StoreApp& app : apps) {
        if (found.candidates.size() == mostCandidates) {
            break;
        }
        found.candidates.push_back(
            Candidate{.appId = std::move(app.appId), .title = std::move(app.title)});
    }
    publish(found);
    for (Candidate& candidate : found.candidates) {
        if (stop.stop_requested()) {
            return;
        }
        std::string dated;
        if (const std::optional<std::string> details =
                web.get(library::matches::releaseDateUrl(candidate.appId), dated)) {
            candidate.year = library::matches::parseReleaseYear(*details, candidate.appId);
        }
        candidate.cover = coverOf(candidate.appId);
        candidate.complete = true;
        publish(found);
    }
}

void GameMatchController::service() {
    std::optional<Found> found;
    {
        const std::lock_guard lock{mutex_};
        found = std::exchange(found_, std::nullopt);
    }
    if (!found || found->generation != generation_) {
        return;
    }
    candidates_ = std::move(found->candidates);
    searched_ = found->searched;
    if (!found->term.empty()) {
        term_ = std::move(found->term);
    }
    error_ = std::move(found->error);
}

std::string GameMatchController::status() const {
    if (!searched_) {
        return "Searching Steam for \"" + term_ + "\"...";
    }
    if (!error_.empty()) {
        return error_;
    }
    if (candidates_.empty()) {
        return "Steam has no game called \"" + term_ + "\"; search another name";
    }
    return std::to_string(candidates_.size()) + " games on Steam for \"" + term_ + "\"";
}

void GameMatchController::choose(std::size_t index) {
    if (index >= candidates_.size()) {
        return;
    }
    const Candidate picked = candidates_[index];
    close();
    if (picked.appId == game_.artworkKey) {
        return;
    }
    lucent::info("own-games", "{} matched to Steam app {} ({})", game_.id, picked.appId,
                 picked.title);
    hooks_.matched(game_, picked.appId, picked.title);
}

void GameMatchController::searchAnother() {
    name_ = term_;
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = nameCardId,
                  .title = "Search another name",
                  .note = "The name Steam's store is searched for.",
                  .label = "Name",
                  .placeholder = "Name",
                  .secret = false,
                  .mostBytes = nameMostLength,
                  .text = [this]() -> const std::string& {
                      return name_;
                  },
                  .changed =
                      [this](std::string text) {
                          name_ = std::move(text);
                      },
                  .submitted =
                      [this] {
                          if (name_.empty()) {
                              hooks_.say("Type a name to search for", true);
                              return;
                          }
                          closeName();
                          search(name_);
                      },
                  .cancelled =
                      [this] {
                          closeName();
                      }}));
    keyboard_.open();
}

void GameMatchController::closeName() {
    for (const gpu2d::ui::Screen* shown : navigator_.screens()) {
        if (shown->id() == nameCardId) {
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*shown);
            return;
        }
    }
}

} // namespace opensu::app
