#include "artwork_fetcher.hpp"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

#include "libretro_index.hpp"
#include "lucent/log.h"
#include "rom_systems.hpp"

namespace opensu::artwork {
namespace {

bool isNotFound(const std::string& error) {
    return error == "HTTP 404";
}

/// GOG's size suffix for the library tile, the fallback when gamesdb has no portrait cover.
constexpr std::string_view gogTileSuffix = "_196.jpg";

/// The portrait cover URL in a gamesdb release answer, `game.vertical_cover.url_format` with its
/// `{formatter}` and `{ext}` filled in; empty when the answer has none.
std::string gogCoverUrl(const std::string& body) {
    const nlohmann::json document = nlohmann::json::parse(body, nullptr, false);
    if (!document.is_object() || !document.contains("game") || !document["game"].is_object()) {
        return {};
    }
    const nlohmann::json& game = document["game"];
    if (!game.contains("vertical_cover") || !game["vertical_cover"].is_object()) {
        return {};
    }
    std::string url = game["vertical_cover"].value("url_format", "");
    const auto fill = [&url](std::string_view key, std::string_view value) {
        for (size_t at = url.find(key); at != std::string::npos; at = url.find(key, at)) {
            url.replace(at, key.size(), value);
            at += value.size();
        }
    };
    fill("{formatter}", "_glx_vertical_cover");
    fill("{ext}", "jpg");
    return url;
}

} // namespace

ArtworkFetcher::ArtworkFetcher(const ArtworkStore& store, const RemoteSources& sources,
                               library::roms::NameDb names)
    : store_{store}, sources_{sources}, names_{std::move(names)},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

ArtworkFetcher::~ArtworkFetcher() {
    worker_.request_stop();
    wake_.notify_all();
}

void ArtworkFetcher::request(const std::vector<library::Game>& games) {
    const ArtworkStore::Clock::time_point now = ArtworkStore::Clock::now();
    std::vector<Work> wanted;
    for (const library::Game& game : games) {
        if (store_.wanted(game, now)) {
            wanted.emplace_back(game);
        }
    }
    const bool arcade = std::ranges::any_of(games, [](const library::Game& game) {
        return game.source == library::Source::Rom && game.sourceId == "arcade";
    });
    if (arcade && names_.enabled() && !names_.cached()) {
        wanted.emplace_back(NamesWork{});
    }
    {
        const std::lock_guard lock{mutex_};
        queue_ = std::move(wanted);
    }
    wake_.notify_all();
}

std::vector<Fetched> ArtworkFetcher::take() {
    const std::lock_guard lock{mutex_};
    return std::exchange(done_, {});
}

void ArtworkFetcher::prioritize(const std::vector<std::string>& gameIds) {
    const std::lock_guard lock{mutex_};
    std::size_t front = 0;
    for (const std::string& id : gameIds) {
        const auto found = std::find_if(queue_.begin() + static_cast<std::ptrdiff_t>(front),
                                        queue_.end(), [&id](const Work& work) {
                                            const auto* game = std::get_if<library::Game>(&work);
                                            return game != nullptr && game->id == id;
                                        });
        if (found != queue_.end()) {
            std::rotate(queue_.begin() + static_cast<std::ptrdiff_t>(front), found, found + 1);
            ++front;
        }
    }
}

Stage ArtworkFetcher::stageOf(std::string_view gameId) const {
    const std::lock_guard lock{mutex_};
    if (downloading_ == gameId) {
        return Stage::Downloading;
    }
    const bool queued = std::ranges::any_of(queue_, [gameId](const Work& work) {
        const auto* game = std::get_if<library::Game>(&work);
        return game != nullptr && game->id == gameId;
    });
    return queued ? Stage::Queued : Stage::None;
}

std::vector<std::string> ArtworkFetcher::pendingGames() const {
    const std::lock_guard lock{mutex_};
    std::vector<std::string> ids;
    if (!downloading_.empty()) {
        ids.push_back(downloading_);
    }
    for (const Work& work : queue_) {
        if (const auto* game = std::get_if<library::Game>(&work)) {
            ids.push_back(game->id);
        }
    }
    return ids;
}

bool ArtworkFetcher::idle() const {
    const std::lock_guard lock{mutex_};
    return queue_.empty() && !busy_;
}

void ArtworkFetcher::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        Work item;
        {
            std::unique_lock lock{mutex_};
            busy_ = false;
            if (!wake_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            item = std::move(queue_.front());
            queue_.erase(queue_.begin());
            busy_ = true;
            if (const auto* game = std::get_if<library::Game>(&item)) {
                downloading_ = game->id;
            }
        }
        const Outcome outcome = std::visit(
            [this](const auto& work) {
                return fetch(work);
            },
            item);
        if (outcome == Outcome::Missing) {
            recordMiss(item);
        }
        const std::lock_guard lock{mutex_};
        downloading_.clear();
        if (outcome == Outcome::Saved) {
            done_.push_back(arrived(item));
            continue;
        }
        if (const auto* game = std::get_if<library::Game>(&item)) {
            done_.push_back(Fetched{Fetched::Kind::Game, game->id, {}, false});
        }
        if (outcome == Outcome::Unreachable) {
            // The network or the cache is down; the next request tries again.
            for (const Work& dropped : queue_) {
                if (const auto* game = std::get_if<library::Game>(&dropped)) {
                    done_.push_back(Fetched{Fetched::Kind::Game, game->id, {}, false});
                }
            }
            queue_.clear();
        }
    }
}

Fetched ArtworkFetcher::arrived(const Work& work) const {
    if (const auto* game = std::get_if<library::Game>(&work)) {
        return Fetched{Fetched::Kind::Game, game->id, store_.pathFor(*game)};
    }
    return Fetched{Fetched::Kind::Names, {}, {}};
}

void ArtworkFetcher::recordMiss(const Work& work) const {
    if (const auto* game = std::get_if<library::Game>(&work)) {
        store_.recordMiss(*game);
    }
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const library::Game& game) {
    switch (game.source) {
    case library::Source::Steam:
        return fetchSteam(game, game.sourceId);
    case library::Source::Rom:
        return fetchRom(game);
    case library::Source::Epic:
        return game.artworkUrl.empty() ? Outcome::Missing : keep(game, game.artworkUrl);
    case library::Source::Gog:
        return fetchGog(game);
    case library::Source::Own:
        return game.artworkKey.empty() ? Outcome::Missing : fetchSteam(game, game.artworkKey);
    }
    return Outcome::Missing;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const NamesWork& /*names*/) {
    std::string error;
    const std::optional<library::roms::NameMap> names =
        fetchArcadeNames(web_, sources_.libretroDatabase, sources_.arcadeDats, error);
    if (!names || !names_.save(*names, error)) {
        lucent::warn("artwork", "arcade names: {}", error);
        return Outcome::Unreachable;
    }
    lucent::info("artwork", "{} arcade names kept", names->size());
    return Outcome::Saved;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchSteam(const library::Game& game,
                                                   const std::string& appId) {
    const std::string base = sources_.steam + "/" + appId + "/";
    // The library portrait, else the store header, which every app has.
    const Outcome portrait = keep(game, base + "library_600x900.jpg");
    if (portrait != Outcome::Missing) {
        return portrait;
    }
    return keep(game, base + "header.jpg");
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchGog(const library::Game& game) {
    if (game.sourceId.empty()) {
        return Outcome::Missing;
    }
    // gamesdb's portrait cover, else the library's own tile.
    const std::string url = sources_.gogdb + "/" + game.sourceId;
    std::string error;
    const std::optional<std::string> release = web_.get(url, error);
    if (release) {
        const std::string cover = gogCoverUrl(*release);
        if (!cover.empty()) {
            const Outcome portrait = keep(game, cover);
            if (portrait != Outcome::Missing) {
                return portrait;
            }
        }
    } else if (!isNotFound(error)) {
        lucent::warn("artwork", "{}: {}", url, error);
        return Outcome::Unreachable;
    }
    return game.artworkUrl.empty() ? Outcome::Missing
                                   : keep(game, game.artworkUrl + std::string{gogTileSuffix});
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchRom(const library::Game& game) {
    const library::roms::RomSystem* system = library::roms::systemByKey(game.sourceId);
    if (system == nullptr || system->libretroName.empty()) {
        return Outcome::Missing;
    }
    const std::vector<std::string>* names = index(*system);
    if (names == nullptr) {
        return Outcome::Unreachable;
    }
    const std::optional<std::string> match = bestMatch(game.artworkKey, *names);
    if (!match) {
        return Outcome::Missing;
    }
    return keep(game, sources_.libretro + "/" + encodeSegment(system->libretroName) +
                          "/Named_Boxarts/" + encodeSegment(*match) + ".png");
}

ArtworkFetcher::Outcome ArtworkFetcher::keep(const library::Game& game, const std::string& url) {
    std::string error;
    const std::optional<std::string> bytes = web_.get(url, error);
    if (!bytes) {
        if (isNotFound(error)) {
            return Outcome::Missing;
        }
        lucent::warn("artwork", "{}: {}", url, error);
        return Outcome::Unreachable;
    }
    if (!store_.save(game, *bytes, error)) {
        lucent::warn("artwork", "{}", error);
        return Outcome::Unreachable;
    }
    return Outcome::Saved;
}

const std::vector<std::string>* ArtworkFetcher::index(const library::roms::RomSystem& rom) {
    const std::string_view system = rom.key;
    const std::string_view libretroName = rom.libretroName;
    if (const auto found = indexes_.find(system); found != indexes_.end()) {
        return &found->second;
    }
    std::optional<std::vector<std::string>> names =
        store_.index(system, ArtworkStore::Clock::now());
    if (!names) {
        std::string error;
        const std::string url =
            sources_.libretro + "/" + encodeSegment(libretroName) + "/Named_Boxarts/";
        const std::optional<std::string> html = web_.get(url, error);
        if (!html) {
            lucent::warn("artwork", "{}: {}", url, error);
            return nullptr;
        }
        names = parseIndex(*html);
        store_.saveIndex(system, *names);
        lucent::info("artwork", "{} box art names listed for {}", names->size(), libretroName);
    }
    return &indexes_.emplace(std::string{system}, std::move(*names)).first->second;
}

} // namespace opensu::artwork
