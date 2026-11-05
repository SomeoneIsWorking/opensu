#include "artwork_fetcher.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "game_icon.hpp"
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

RemoteSources RemoteSources::of(const config::Config& config) {
    RemoteSources sources;
    if (!config.libretroThumbnails.empty()) {
        sources.libretro = config.libretroThumbnails;
    }
    if (!config.steamStandIn.empty()) {
        sources.steam = config.steamStandIn + "/cdn";
        sources.steamStore = config.steamStandIn;
        sources.steamItems = config.steamStandIn + "/items";
        sources.steamAssets = config.steamStandIn + "/assets/";
    }
    sources.iconThemes = {config.xdgDataHome / "icons", "/usr/local/share/icons",
                          "/usr/share/icons"};
    return sources;
}

net::Result<std::vector<std::string>> steamArtUrls(const std::stop_token& stop,
                                                   const net::WebClient& web,
                                                   const RemoteSources& sources,
                                                   const std::string& appId) {
    std::uint32_t app = 0;
    std::from_chars(appId.data(), appId.data() + appId.size(), app);
    const std::string url = library::steam_items::itemsUrl({app}, sources.steamItems);
    const net::Result<std::string> answer = web.get(stop, url);
    if (!answer) {
        return answer.as<std::vector<std::string>>("GetItems: ");
    }
    std::vector<std::string> urls;
    try {
        for (const library::steam_items::Item& item :
             library::steam_items::parseItems(*answer, sources.steamAssets)) {
            for (const std::string& art : {item.portraitUrl, item.headerUrl}) {
                if (!art.empty()) {
                    urls.push_back(art);
                }
            }
        }
    } catch (const std::runtime_error& error) {
        lucent::warn("artwork", "Steam app {}: {}", appId, error.what());
    }
    return urls;
}

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
        if (store_.wanted(game, now) || store_.wantsIcon(game)) {
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
            [this, &stop](const auto& work) {
                return fetch(work, stop);
            },
            item);
        if (outcome == Outcome::Stopped) {
            return;
        }
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

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const library::Game& game,
                                              const std::stop_token& stop) {
    if (game.source == library::Source::Own) {
        return fetchOwn(game, stop);
    }
    if (game.source != library::Source::Rom && !game.artworkKey.empty()) {
        // Matched to a Steam app: its covers, whatever the game's own source.
        return fetchSteam(game, game.artworkKey, stop);
    }
    switch (game.source) {
    case library::Source::Steam:
        return fetchSteam(game, game.sourceId, stop);
    case library::Source::Rom:
        return fetchRom(game, stop);
    case library::Source::Epic:
        return game.artworkUrl.empty() ? Outcome::Missing : keep(game, game.artworkUrl, stop);
    case library::Source::Gog:
        return fetchGog(game, stop);
    case library::Source::Own:
    // A remote game's host sends its cover with the listing.
    case library::Source::Remote:
        return Outcome::Missing;
    }
    return Outcome::Missing;
}

void ArtworkFetcher::keepIcon(const library::Game& game) {
    if (!store_.wantsIcon(game)) {
        return;
    }
    const std::optional<std::string> png = gameIcon(game, sources_.iconThemes);
    if (!png) {
        return;
    }
    std::string error;
    if (!store_.saveIcon(game, *png, error)) {
        lucent::warn("artwork", "{}", error);
        return;
    }
    const std::lock_guard lock{mutex_};
    done_.push_back(Fetched{Fetched::Kind::Icon, game.id, store_.iconFor(game)});
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchOwn(const library::Game& game,
                                                 const std::stop_token& stop) {
    keepIcon(game);
    if (!store_.wanted(game, ArtworkStore::Clock::now())) {
        return Outcome::Settled;
    }
    if (!game.boxArt.empty()) {
        return fetchBoxPick(game, stop);
    }
    std::string appId = game.artworkKey;
    if (appId.empty()) {
        const std::string url = library::identify::storeSearchUrl(game.title, sources_.steamStore);
        const net::Result<std::string> answer = web_.get(stop, url);
        if (answer.stopped()) {
            return Outcome::Stopped;
        }
        if (!answer) {
            lucent::warn("artwork", "{}: {}", url, answer.error());
            return Outcome::Unreachable;
        }
        if (const std::optional<library::identify::StoreApp> app = library::identify::sameTitle(
                library::identify::parseStoreSearch(*answer), game.title)) {
            appId = app->appId;
        }
    }
    if (!appId.empty()) {
        const Outcome steam = fetchSteam(game, appId, stop);
        if (steam != Outcome::Missing) {
            return steam;
        }
    }
    return fetchBoxArt(game, stop);
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchBoxPick(const library::Game& game,
                                                     const std::stop_token& stop) {
    const std::size_t slash = game.boxArt.find('/');
    const library::roms::RomSystem* system =
        slash == std::string::npos ? nullptr
                                   : library::roms::systemByKey(game.boxArt.substr(0, slash));
    if (system == nullptr) {
        return Outcome::Missing;
    }
    return keep(game, boxArtUrl(sources_.libretro, *system, game.boxArt.substr(slash + 1)), stop);
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchBoxArt(const library::Game& game,
                                                    const std::stop_token& stop) {
    for (const std::string_view key : boxArtSystems) {
        const library::roms::RomSystem* system = library::roms::systemByKey(key);
        if (system == nullptr) {
            continue;
        }
        const std::vector<std::string>* names = index(*system, stop);
        if (stop.stop_requested()) {
            return Outcome::Stopped;
        }
        if (names == nullptr) {
            return Outcome::Unreachable;
        }
        const std::optional<std::string> match = confidentMatch(game.title, *names);
        if (!match) {
            continue;
        }
        lucent::info("artwork", "{} takes box art {} of {}", game.id, *match, key);
        const Outcome outcome = keep(game, boxArtUrl(sources_.libretro, *system, *match), stop);
        if (outcome != Outcome::Missing) {
            return outcome;
        }
    }
    return Outcome::Missing;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const NamesWork& /*names*/,
                                              const std::stop_token& stop) {
    const net::Result<library::roms::NameMap> names =
        fetchArcadeNames(stop, web_, sources_.libretroDatabase, sources_.arcadeDats);
    if (names.stopped()) {
        return Outcome::Stopped;
    }
    std::string error = names.error();
    if (!names || !names_.save(*names, error)) {
        lucent::warn("artwork", "arcade names: {}", error);
        return Outcome::Unreachable;
    }
    lucent::info("artwork", "{} arcade names kept", names->size());
    return Outcome::Saved;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchSteam(const library::Game& game,
                                                   const std::string& appId,
                                                   const std::stop_token& stop) {
    const std::string base = sources_.steam + "/" + appId + "/";
    // The library portrait, else the store header, which every app has.
    const Outcome portrait = keep(game, base + "library_600x900.jpg", stop);
    if (portrait != Outcome::Missing) {
        return portrait;
    }
    const Outcome header = keep(game, base + "header.jpg", stop);
    if (header != Outcome::Missing) {
        return header;
    }
    const net::Result<std::vector<std::string>> urls = steamArtUrls(stop, web_, sources_, appId);
    if (urls.stopped()) {
        return Outcome::Stopped;
    }
    if (!urls) {
        lucent::warn("artwork", "{}", urls.error());
        return Outcome::Unreachable;
    }
    for (const std::string& url : *urls) {
        const Outcome art = keep(game, url, stop);
        if (art != Outcome::Missing) {
            return art;
        }
    }
    return Outcome::Missing;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchGog(const library::Game& game,
                                                 const std::stop_token& stop) {
    if (game.sourceId.empty()) {
        return Outcome::Missing;
    }
    // gamesdb's portrait cover, else the library's own tile.
    const std::string url = sources_.gogdb + "/" + game.sourceId;
    const net::Result<std::string> release = web_.get(stop, url);
    if (release.stopped()) {
        return Outcome::Stopped;
    }
    if (release) {
        const std::string cover = gogCoverUrl(*release);
        if (!cover.empty()) {
            const Outcome portrait = keep(game, cover, stop);
            if (portrait != Outcome::Missing) {
                return portrait;
            }
        }
    } else if (!isNotFound(release.error())) {
        lucent::warn("artwork", "{}: {}", url, release.error());
        return Outcome::Unreachable;
    }
    return game.artworkUrl.empty() ? Outcome::Missing
                                   : keep(game, game.artworkUrl + std::string{gogTileSuffix}, stop);
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchRom(const library::Game& game,
                                                 const std::stop_token& stop) {
    const library::roms::RomSystem* system = library::roms::systemByKey(game.sourceId);
    if (system == nullptr || system->libretroName.empty()) {
        return Outcome::Missing;
    }
    const std::vector<std::string>* names = index(*system, stop);
    if (stop.stop_requested()) {
        return Outcome::Stopped;
    }
    if (names == nullptr) {
        return Outcome::Unreachable;
    }
    const std::optional<std::string> match = bestMatch(game.artworkKey, *names);
    if (!match) {
        return Outcome::Missing;
    }
    return keep(game, boxArtUrl(sources_.libretro, *system, *match), stop);
}

ArtworkFetcher::Outcome ArtworkFetcher::keep(const library::Game& game, const std::string& url,
                                             const std::stop_token& stop) {
    const net::Result<std::string> bytes = web_.get(stop, url);
    if (bytes.stopped()) {
        return Outcome::Stopped;
    }
    if (!bytes) {
        if (isNotFound(bytes.error())) {
            return Outcome::Missing;
        }
        lucent::warn("artwork", "{}: {}", url, bytes.error());
        return Outcome::Unreachable;
    }
    std::string error;
    if (!store_.save(game, *bytes, error)) {
        lucent::warn("artwork", "{}", error);
        return Outcome::Unreachable;
    }
    return Outcome::Saved;
}

const std::vector<std::string>* ArtworkFetcher::index(const library::roms::RomSystem& rom,
                                                      const std::stop_token& stop) {
    const std::string_view system = rom.key;
    if (const auto found = indexes_.find(system); found != indexes_.end()) {
        return &found->second;
    }
    net::Result<std::vector<std::string>> names =
        boxArtNames(stop, web_, store_, rom, sources_.libretro);
    if (names.stopped()) {
        return nullptr;
    }
    if (!names) {
        lucent::warn("artwork", "{}", names.error());
        return nullptr;
    }
    return &indexes_.emplace(std::string{system}, *std::move(names)).first->second;
}

} // namespace opensu::artwork
