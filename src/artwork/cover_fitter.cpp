#include "cover_fitter.hpp"

#include <optional>
#include <utility>

#include "gpu2d/image/decode.hpp"
#include "lucent/log.h"

namespace opensu::artwork {
namespace {

/// The SteamGridDB lookup of `game`: by Steam app id for a Steam game, and by title.
SteamGridDbGame lookupOf(const library::Game& game) {
    return {game.source == library::Source::Steam ? game.sourceId : "", game.title};
}

} // namespace

CoverFitter::CoverFitter(const CoverOverrides& overrides, const SteamGridDb& steamGridDb)
    : overrides_{overrides}, steamGridDb_{steamGridDb},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

CoverFitter::~CoverFitter() {
    worker_.request_stop();
    wake_.notify_all();
}

bool CoverFitter::wanted(const library::Game& game) const {
    return !game.artwork.empty() && steamGridDb_.connected() &&
           overrides_.wantsFit(game.id, ArtworkStore::Clock::now());
}

void CoverFitter::request(const std::vector<library::Game>& games, CoverShape shape) {
    std::vector<Work> wantedNow;
    for (const library::Game& game : games) {
        if (wanted(game)) {
            wantedNow.push_back({game, shape});
        }
    }
    {
        const std::lock_guard lock{mutex_};
        queue_ = std::move(wantedNow);
    }
    wake_.notify_all();
}

void CoverFitter::check(const library::Game& game, CoverShape shape) {
    if (!wanted(game)) {
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        queue_.push_back({game, shape});
    }
    wake_.notify_all();
}

std::vector<FittedCover> CoverFitter::take() {
    const std::lock_guard lock{mutex_};
    return std::exchange(done_, {});
}

bool CoverFitter::idle() const {
    const std::lock_guard lock{mutex_};
    return queue_.empty() && !busy_;
}

void CoverFitter::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        Work work;
        {
            std::unique_lock lock{mutex_};
            busy_ = false;
            if (!wake_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            work = std::move(queue_.front());
            queue_.erase(queue_.begin());
            busy_ = true;
        }
        if (!fit(work)) {
            const std::lock_guard lock{mutex_};
            queue_.clear();
        }
    }
}

bool CoverFitter::fit(const Work& work) {
    const library::Game& game = work.game;
    const std::string seen = game.id + "\n" + game.artwork.string();
    if (fits_.contains(seen)) {
        return true;
    }
    std::string error;
    const std::optional<gpu2d::image::Bitmap> cover = gpu2d::image::decodeFile(game.artwork, error);
    if (!cover) {
        lucent::warn("artwork", "{} cover unreadable: {}", game.id, error);
        return true;
    }
    if (coverShapeOf(cover->width, cover->height) == work.shape) {
        fits_.insert(seen);
        return true;
    }
    const SteamGridDb::Covers offered = steamGridDb_.covers(web_, lookupOf(game), work.shape);
    switch (offered.status) {
    case SteamGridDb::Covers::Status::Disconnected:
    case SteamGridDb::Covers::Status::Refused:
        return false;
    case SteamGridDb::Covers::Status::Unreachable:
        lucent::warn("artwork", "{}", offered.error);
        return false;
    case SteamGridDb::Covers::Status::Listed:
        break;
    }
    for (const SteamGridDbCover& candidate : offered.covers) {
        const std::optional<std::string> bytes = web_.get(candidate.url, error);
        if (!bytes) {
            if (error == "HTTP 404") {
                continue;
            }
            lucent::warn("artwork", "{}: {}", candidate.url, error);
            return false;
        }
        if (!overrides_.fit(game.id, *bytes, error)) {
            lucent::warn("artwork", "{}", error);
            return false;
        }
        lucent::info("artwork", "{}: fitted SteamGridDB cover {} ({}x{})", game.id, candidate.id,
                     candidate.width, candidate.height);
        const std::lock_guard lock{mutex_};
        done_.push_back({game.id, overrides_.shown(game.id)});
        return true;
    }
    overrides_.recordFitMiss(game.id);
    return true;
}

} // namespace opensu::artwork
