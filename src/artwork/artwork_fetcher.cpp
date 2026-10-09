#include "artwork_fetcher.hpp"

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

ArtworkFetcher::ArtworkFetcher(const ArtworkStore& store, const RemoteSources& sources)
    : store_{store}, sources_{sources}, apk_{sources.iisuApk, sources.iisuPin.apkSize},
      pack_{store, apk_, sources.iisuPin}, glyphs_{apk_},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

ArtworkFetcher::~ArtworkFetcher() {
    worker_.request_stop();
    wake_.notify_all();
}

void ArtworkFetcher::request(const std::vector<library::Game>& games,
                             const std::vector<library::Console>& consoles,
                             const std::vector<ApkAsset>& assets) {
    const ArtworkStore::Clock::time_point now = ArtworkStore::Clock::now();
    std::vector<Work> wanted;
    for (const ApkAsset& asset : assets) {
        if (store_.wantedAsset(asset, now)) {
            wanted.emplace_back(asset);
        }
    }
    for (const library::Console& console : consoles) {
        if (store_.wanted(console, now)) {
            wanted.emplace_back(console);
        }
    }
    for (const library::Console& console : consoles) {
        if (store_.wantedGlyph(console.system, now)) {
            wanted.emplace_back(GlyphWork{console.system});
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
        }
        const Outcome outcome = std::visit(
            [this](const auto& work) {
                return fetch(work);
            },
            item);
        if (outcome == Outcome::Saved) {
            const std::lock_guard lock{mutex_};
            done_.push_back(arrived(item));
        } else if (outcome == Outcome::Missing) {
            recordMiss(item);
        } else {
            // The network or the cache is down; the next request tries again.
            const std::lock_guard lock{mutex_};
            queue_.clear();
        }
    }
}

Fetched ArtworkFetcher::arrived(const Work& work) const {
    if (const auto* game = std::get_if<library::Game>(&work)) {
        return Fetched{Fetched::Kind::Game, game->id, store_.pathFor(*game)};
    }
    if (const auto* console = std::get_if<library::Console>(&work)) {
        return Fetched{Fetched::Kind::Console, console->system, store_.pathFor(*console)};
    }
    if (const auto* glyph = std::get_if<GlyphWork>(&work)) {
        return Fetched{Fetched::Kind::Glyph, glyph->system, store_.glyphPath(glyph->system)};
    }
    const ApkAsset& asset = std::get<ApkAsset>(work);
    return Fetched{asset.kind == AssetKind::Sound ? Fetched::Kind::Sound : Fetched::Kind::NavIcon,
                   asset.file, store_.assetPath(asset)};
}

void ArtworkFetcher::recordMiss(const Work& work) const {
    if (const auto* game = std::get_if<library::Game>(&work)) {
        store_.recordMiss(*game);
    } else if (const auto* console = std::get_if<library::Console>(&work)) {
        store_.recordMiss(*console);
    } else if (const auto* glyph = std::get_if<GlyphWork>(&work)) {
        store_.recordGlyphMiss(glyph->system);
    } else {
        store_.recordAssetMiss(std::get<ApkAsset>(work));
    }
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const library::Game& game) {
    switch (game.source) {
    case library::Source::Steam:
        return fetchSteam(game);
    case library::Source::Rom:
        return fetchRom(game);
    case library::Source::Epic:
        return game.artworkUrl.empty() ? Outcome::Missing : keep(game, game.artworkUrl);
    case library::Source::Gog:
        return fetchGog(game);
    }
    return Outcome::Missing;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const library::Console& console) {
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

ArtworkFetcher::Outcome
ArtworkFetcher::kept(const ApkFile& file, std::string_view what,
                     const std::function<bool(std::string_view, std::string&)>& keep) {
    switch (file.status) {
    case ApkFile::Status::Missing:
        return Outcome::Missing;
    case ApkFile::Status::Failed:
        lucent::warn("artwork", "{}: {}", what, file.error);
        return Outcome::Unreachable;
    case ApkFile::Status::Found:
        break;
    }
    std::string error;
    if (!keep(file.bytes, error)) {
        lucent::warn("artwork", "{}", error);
        return Outcome::Unreachable;
    }
    return Outcome::Saved;
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const GlyphWork& glyph) {
    return kept(glyphs_.fetch(web_, glyph.system), glyph.system + " glyph",
                [&](std::string_view bytes, std::string& error) {
                    return store_.saveGlyph(glyph.system, bytes, error);
                });
}

ArtworkFetcher::Outcome ArtworkFetcher::fetch(const ApkAsset& asset) {
    return kept(apk_.fetch(web_, asset.entry), asset.file,
                [&](std::string_view bytes, std::string& error) {
                    return store_.saveAsset(asset, bytes, error);
                });
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
