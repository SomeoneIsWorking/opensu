#include "cover_fitter.hpp"

#include <chrono>
#include <optional>
#include <utility>

#include "gpu2d/image/decode.hpp"
#include "lucent/log.h"

namespace opensu::artwork {
namespace {

/// How long the fitter waits before asking an unreachable SteamGridDB again.
constexpr std::chrono::seconds retryPause{30};

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
    const bool coverless = game.source == library::Source::Own && game.artwork.empty();
    return (coverless || !game.artwork.empty()) && steamGridDb_.connected() &&
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
        const Outcome outcome = fit(work);
        std::unique_lock lock{mutex_};
        if (outcome == Outcome::Stop) {
            queue_.clear();
        } else if (outcome == Outcome::Retry) {
            work.retried = true;
            queue_.push_back(std::move(work));
            wake_.wait_for(lock, stop, retryPause, [] {
                return false;
            });
        }
    }
}

CoverFitter::Outcome CoverFitter::fit(const Work& work) {
    const library::Game& game = work.game;
    const std::string seen = game.id + "\n" + game.artwork.string();
    // A later request can queue a game again while its fit is under way.
    if (fits_.contains(seen) || !overrides_.wantsFit(game.id, ArtworkStore::Clock::now())) {
        return Outcome::Next;
    }
    std::string error;
    if (!game.artwork.empty()) {
        const std::optional<gpu2d::image::Bitmap> cover =
            gpu2d::image::decodeFile(game.artwork, error);
        if (!cover) {
            lucent::warn("artwork", "{} cover unreadable: {}", game.id, error);
            return Outcome::Next;
        }
        if (coverShapeOf(cover->width, cover->height) == work.shape) {
            fits_.insert(seen);
            return Outcome::Next;
        }
    }
    const SteamGridDb::Covers offered =
        steamGridDb_.covers(web_, steamGridDbGameOf(game), work.shape);
    switch (offered.status) {
    case SteamGridDb::Covers::Status::Disconnected:
    case SteamGridDb::Covers::Status::Refused:
        return Outcome::Stop;
    case SteamGridDb::Covers::Status::Unreachable:
        lucent::warn("artwork", "{}", offered.error);
        return work.retried ? Outcome::Next : Outcome::Retry;
    case SteamGridDb::Covers::Status::Listed:
        break;
    }
    for (const SteamGridDbCover& candidate : offered.covers) {
        const std::optional<std::string> bytes = web_.get(candidate.url, error);
        if (!bytes) {
            if (error == "HTTP 404") {
                continue;
            }
            // One image the CDN did not deliver: this game is asked again next round.
            lucent::warn("artwork", "{}: {}: {}", game.id, candidate.url, error);
            return Outcome::Next;
        }
        if (!overrides_.fit(game.id, *bytes, error)) {
            lucent::warn("artwork", "{}", error);
            return Outcome::Stop;
        }
        lucent::info("artwork", "{}: fitted SteamGridDB cover {} ({}x{})", game.id, candidate.id,
                     candidate.width, candidate.height);
        const std::lock_guard lock{mutex_};
        done_.push_back({game.id, overrides_.shown(game.id)});
        return Outcome::Next;
    }
    overrides_.recordFitMiss(game.id);
    return Outcome::Next;
}

} // namespace opensu::artwork
