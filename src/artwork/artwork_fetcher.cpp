#include "artwork_fetcher.hpp"

#include <utility>

#include "libretro_index.hpp"
#include "lucent/log.h"
#include "rom_systems.hpp"

namespace iideck::artwork {
namespace {

bool isNotFound(const std::string& error) {
    return error == "HTTP 404";
}

} // namespace

ArtworkFetcher::ArtworkFetcher(const ArtworkStore& store, const RemoteSources& sources)
    : store_{store}, sources_{sources}, pack_{store, sources.iisuApk, sources.iisuPin},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

ArtworkFetcher::~ArtworkFetcher() {
    worker_.request_stop();
    wake_.notify_all();
}

void ArtworkFetcher::request(const std::vector<library::Game>& games,
                             const std::vector<library::Console>& consoles) {
    const ArtworkStore::Clock::time_point now = ArtworkStore::Clock::now();
    std::vector<library::ShelfItem> wanted;
    for (const library::Console& console : consoles) {
        if (store_.wanted(console, now)) {
            wanted.emplace_back(console);
        }
    }
    for (const library::Game& game : games) {
        if (store_.wanted(game, now)) {
            wanted.emplace_back(game);
        }
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

bool ArtworkFetcher::idle() const {
    const std::lock_guard lock{mutex_};
    return queue_.empty() && !busy_;
}

void ArtworkFetcher::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        library::ShelfItem item;
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
        }
        const auto* game = std::get_if<library::Game>(&item);
        const auto* console = std::get_if<library::Console>(&item);
        const Outcome outcome = game != nullptr ? fetch(*game) : fetchConsole(*console);
        if (outcome == Outcome::Saved) {
            const std::lock_guard lock{mutex_};
            done_.push_back(
                game != nullptr
                    ? Fetched{Fetched::Kind::Game, game->id, store_.pathFor(*game)}
                    : Fetched{Fetched::Kind::Console, console->system, store_.pathFor(*console)});
        } else if (outcome == Outcome::Missing) {
            if (game != nullptr) {
                store_.recordMiss(*game);
            } else {
                store_.recordMiss(*console);
            }
        } else {
            // The network or the cache is down; the next request tries again.
            const std::lock_guard lock{mutex_};
            queue_.clear();
        }
    }
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const library::Game& game) {
    switch (game.source) {
    case library::Source::Steam:
        return fetchSteam(game);
    case library::Source::Rom:
        return fetchRom(game);
    case library::Source::Epic:
    case library::Source::Gog:
        break;
    }
    return Outcome::Missing;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchConsole(const library::Console& console) {
    std::string error;
    if (!pack_.ensure(web_, error)) {
        lucent::warn("artwork", "starter pack: {}", error);
        return Outcome::Unreachable;
    }
    const std::optional<std::string> png = pack_.card(console.system, error);
    if (!png) {
        if (!error.empty()) {
            lucent::warn("artwork", "{} card: {}", console.system, error);
        }
        return Outcome::Missing;
    }
    if (!store_.save(console, *png, error)) {
        lucent::warn("artwork", "{}", error);
        return Outcome::Unreachable;
    }
    return Outcome::Saved;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetchSteam(const library::Game& game) {
    const std::string base = sources_.steam + "/" + game.sourceId + "/";
    // The library portrait, else the store header, which every app has.
    const Outcome portrait = keep(game, base + "library_600x900.jpg");
    if (portrait != Outcome::Missing) {
        return portrait;
    }
    return keep(game, base + "header.jpg");
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

} // namespace iideck::artwork
