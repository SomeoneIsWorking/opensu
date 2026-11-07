#include "game_match_controller.hpp"

#include <algorithm>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

#include "artwork/libretro_index.hpp"
#include "fileio/atomic_write.hpp"
#include "game_match_screen.hpp"
#include "library/game_identify.hpp"
#include "library/game_matches.hpp"
#include "library/rom_systems.hpp"
#include "library/titles.hpp"
#include "lucent/log.h"
#include "lucent/text.h"
#include "net/web_client.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;

/// The most candidates one search offers.
constexpr std::size_t mostCandidates = 8;
constexpr const char* nameCardId = "match/name";
constexpr std::size_t nameMostLength = 256;

} // namespace

GameMatchController::GameMatchController(Parts parts, Places places,
                                         const artwork::ArtworkStore& listings, Hooks hooks)
    : navigator_{parts.navigator}, chrome_{parts.chrome}, images_{parts.images},
      keyboard_{parts.keyboard}, sounds_{parts.sounds}, places_{std::move(places)},
      listings_{listings}, hooks_{std::move(hooks)} {
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
    if (isOpen() || library::matchKindOf(game) == library::MatchKind::None) {
        return;
    }
    game_ = game;
    current_ =
        game.boxArt.empty() ? game.artworkKey : std::string{library::boxArtPick} + game.boxArt;
    kind_ = library::matchKindOf(game);
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

void GameMatchController::search(std::string term) {
    term_ = term;
    candidates_.clear();
    searched_ = false;
    error_.clear();
    const std::uint64_t generation = ++generation_;
    finder_ = std::jthread{[this, term = std::move(term), generation](const std::stop_token& stop) {
        find(stop, term, generation);
    }};
}

void GameMatchController::publish(const Found& found) {
    const std::lock_guard lock{mutex_};
    found_ = found;
}

fs::path GameMatchController::cached(const std::stop_token& stop, const std::string& url,
                                     const std::string& name) const {
    fs::path file = places_.covers / name;
    std::error_code ec;
    if (fs::is_regular_file(file, ec)) {
        return file;
    }
    const net::WebClient web;
    const net::Result<std::string> bytes = web.get(stop, url);
    if (!bytes) {
        return {};
    }
    std::string error;
    if (!fileio::writeWhole(file, *bytes, error)) {
        lucent::warn("match", "{}", error);
        return {};
    }
    return file;
}

void GameMatchController::find(const std::stop_token& stop, const std::string& term,
                               std::uint64_t generation) {
    Found found{.generation = generation};
    if (kind_ == library::MatchKind::LibretroTitle) {
        findBoxArt(stop, term, found);
    } else {
        findSteamApps(stop, term, found);
        if (library::matchedAsAdded(game_)) {
            findOwnBoxArt(stop, term, found);
        }
    }
    complete(stop, found);
}

void GameMatchController::findSteamApps(const std::stop_token& stop, const std::string& term,
                                        Found& found) {
    const net::WebClient web;
    std::vector<library::identify::StoreApp> apps;
    // Steam's search wants every word, so a title with a word Steam's name lacks ("Remake")
    // is searched again without its last words.
    for (std::string asked = term; !asked.empty() && apps.empty();) {
        const net::Result<std::string> answer =
            web.get(stop, library::identify::storeSearchUrl(asked, places_.sources.steamStore));
        if (answer.stopped()) {
            return;
        }
        if (!answer) {
            lucent::warn("match", "Steam store search: {}", answer.error());
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
            Candidate{.key = std::move(app.appId), .title = std::move(app.title)});
    }
    publish(found);
}

void GameMatchController::findBoxArt(const std::stop_token& stop, const std::string& term,
                                     Found& found) {
    const library::roms::RomSystem* system =
        library::roms::systemByKey(library::matchSystemOf(game_));
    const auto unlisted = [this, &found](std::string_view why) {
        lucent::warn("match", "libretro-thumbnails listing: {}", why);
        found.searched = true;
        found.error = "libretro-thumbnails cannot be reached";
        publish(found);
    };
    if (system == nullptr) {
        unlisted("no libretro system");
        return;
    }
    const net::WebClient web;
    net::Result<std::vector<std::string>> names =
        artwork::boxArtNames(stop, web, listings_, *system, places_.sources.libretro);
    if (names.stopped()) {
        return;
    }
    if (!names) {
        unlisted(names.error());
        return;
    }
    found.searched = true;
    std::vector<std::string> titles;
    for (std::string asked = term; !asked.empty() && titles.empty();) {
        titles = artwork::namesMatching(asked, *names, mostCandidates);
        found.term = asked;
        const std::size_t space = asked.find_last_of(' ');
        asked.resize(space == std::string::npos ? 0 : space);
    }
    for (std::string& title : titles) {
        found.candidates.push_back(Candidate{
            .key = title,
            .title = title,
            .boxArtUrl = artwork::boxArtUrl(places_.sources.libretro, *system, title),
            .boxArtFile = std::string{system->key} + "-" + artwork::thumbnailName(title) + ".png"});
    }
    publish(found);
}

void GameMatchController::findOwnBoxArt(const std::stop_token& stop, const std::string& term,
                                        Found& found) {
    const net::WebClient web;
    if (found.candidates.empty()) {
        found.term = term;
    }
    std::size_t offered = 0;
    for (const std::string_view key : artwork::boxArtSystems) {
        const library::roms::RomSystem* system = library::roms::systemByKey(key);
        if (system == nullptr || offered == mostCandidates) {
            continue;
        }
        const net::Result<std::vector<std::string>> names =
            artwork::boxArtNames(stop, web, listings_, *system, places_.sources.libretro);
        if (names.stopped()) {
            return;
        }
        if (!names) {
            lucent::warn("match", "libretro-thumbnails listing: {}", names.error());
            continue;
        }
        for (const std::string& title :
             artwork::namesMatching(term, *names, mostCandidates - offered)) {
            found.candidates.push_back(Candidate{
                .key = std::string{library::boxArtPick} + std::string{key} + "/" + title,
                .title = title,
                .boxArtUrl = artwork::boxArtUrl(places_.sources.libretro, *system, title),
                .boxArtFile = std::string{key} + "-" + artwork::thumbnailName(title) + ".png",
                .system = std::string{system->label}});
            ++offered;
        }
        publish(found);
    }
}

fs::path GameMatchController::steamCover(const std::stop_token& stop,
                                         const std::string& app) const {
    const std::string base = places_.sources.steam + "/" + app + "/";
    // The library portrait, else the store header, else the art `GetItems` names.
    fs::path cover = cached(stop, base + "library_600x900.jpg", app + ".jpg");
    if (cover.empty()) {
        cover = cached(stop, base + "header.jpg", app + "-header.jpg");
    }
    if (!cover.empty() || stop.stop_requested()) {
        return cover;
    }
    const net::WebClient web;
    const net::Result<std::vector<std::string>> urls =
        artwork::steamArtUrls(stop, web, places_.sources, app);
    if (urls && !urls->empty()) {
        return cached(stop, urls->front(), app + "-art.jpg");
    }
    return {};
}

void GameMatchController::complete(const std::stop_token& stop, Found& found) {
    const net::WebClient web;
    for (Candidate& candidate : found.candidates) {
        if (stop.stop_requested()) {
            return;
        }
        if (candidate.boxArtUrl.empty()) {
            if (const net::Result<std::string> details =
                    web.get(stop, library::matches::releaseDateUrl(candidate.key,
                                                                   places_.sources.steamStore))) {
                candidate.year = library::matches::parseReleaseYear(*details, candidate.key);
            }
            candidate.cover = steamCover(stop, candidate.key);
        } else {
            candidate.cover = cached(stop, candidate.boxArtUrl, candidate.boxArtFile);
        }
        if (stop.stop_requested()) {
            return;
        }
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

std::string GameMatchController::searched() const {
    if (kind_ == library::MatchKind::LibretroTitle) {
        return "libretro-thumbnails";
    }
    return library::matchedAsAdded(game_) ? "Steam and libretro-thumbnails" : "Steam";
}

std::string GameMatchController::status() const {
    if (!searched_) {
        return "Searching " + searched() + " for \"" + term_ + "\"...";
    }
    if (!error_.empty() && candidates_.empty()) {
        return error_;
    }
    if (candidates_.empty()) {
        return searched() + " has no game called \"" + term_ + "\"; search another name";
    }
    return lucent::text::counted(candidates_.size(), "game") + " on " + searched() + " for \"" +
           term_ + "\"";
}

void GameMatchController::choose(std::size_t index) {
    if (index >= candidates_.size()) {
        return;
    }
    const Candidate picked = candidates_[index];
    close();
    if (picked.key == current_) {
        return;
    }
    lucent::info("match", "{} matched to {} ({})", game_.id, picked.key, picked.title);
    hooks_.matched(game_, picked.key, picked.title);
}

void GameMatchController::searchAnother() {
    name_ = term_;
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = nameCardId,
                  .title = "Search another name",
                  .note = "The name " + searched() + " is searched for.",
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
