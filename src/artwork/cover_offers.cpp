#include "cover_offers.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::artwork {
namespace {

namespace fs = std::filesystem;
using Status = SteamGridDb::Covers::Status;

bool isFile(const fs::path& file) {
    std::error_code ec;
    return fs::is_regular_file(file, ec);
}

} // namespace

CoverOffers::CoverOffers(const CoverOverrides& overrides, const SteamGridDb& steamGridDb,
                         fs::path thumbDir)
    : overrides_{overrides}, steamGridDb_{steamGridDb}, thumbDir_{std::move(thumbDir)},
      worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

CoverOffers::~CoverOffers() {
    worker_.request_stop();
    wake_.notify_all();
}

void CoverOffers::list(const library::Game& game) {
    {
        const std::lock_guard lock{mutex_};
        ++generation_;
        std::erase_if(queue_, [](const Work& work) {
            return std::holds_alternative<List>(work);
        });
        queue_.emplace_back(List{game, generation_});
        listing_.reset();
    }
    wake_.notify_all();
}

void CoverOffers::choose(const std::string& gameId, const SteamGridDbCover& cover) {
    {
        const std::lock_guard lock{mutex_};
        queue_.emplace_back(Choose{gameId, cover});
    }
    wake_.notify_all();
}

void CoverOffers::chooseOwn(const std::string& gameId) {
    {
        const std::lock_guard lock{mutex_};
        queue_.emplace_back(Choose{gameId, std::nullopt});
    }
    wake_.notify_all();
}

std::optional<CoverListing> CoverOffers::takeListing() {
    const std::lock_guard lock{mutex_};
    return std::exchange(listing_, std::nullopt);
}

std::vector<CoverChoice> CoverOffers::takeChoices() {
    const std::lock_guard lock{mutex_};
    return std::exchange(choices_, {});
}

bool CoverOffers::current(std::uint64_t generation) const {
    const std::lock_guard lock{mutex_};
    return generation == generation_;
}

void CoverOffers::publish(const CoverListing& listing, std::uint64_t generation) {
    const std::lock_guard lock{mutex_};
    if (generation == generation_) {
        listing_ = listing;
    }
}

void CoverOffers::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        Work work;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            work = std::move(queue_.front());
            queue_.erase(queue_.begin());
        }
        std::visit(
            [this](const auto& job) {
                perform(job);
            },
            work);
    }
}

void CoverOffers::perform(const List& list) {
    const library::Game& game = list.game;
    const SteamGridDbGame lookup{game.source == library::Source::Steam ? game.sourceId : "",
                                 game.title};
    SteamGridDb::Covers covers = steamGridDb_.covers(web_, lookup, std::nullopt);
    CoverListing listing{game.id, covers.status, {}, covers.error, false};
    if (covers.covers.size() > mostOffers) {
        covers.covers.resize(mostOffers);
    }
    for (SteamGridDbCover& cover : covers.covers) {
        listing.offers.push_back({std::move(cover), {}});
    }
    publish(listing, list.generation);
    for (CoverOffer& offer : listing.offers) {
        if (!current(list.generation)) {
            return;
        }
        const fs::path thumb = thumbDir_ / std::to_string(offer.cover.id);
        if (!isFile(thumb)) {
            std::string error;
            const std::optional<std::string> bytes = web_.get(offer.cover.thumb, error);
            if (!bytes || !fileio::writeWhole(thumb, *bytes, error)) {
                lucent::warn("artwork", "{}: {}", offer.cover.thumb, error);
                continue;
            }
        }
        offer.thumb = thumb;
        publish(listing, list.generation);
    }
    listing.complete = true;
    publish(listing, list.generation);
}

void CoverOffers::perform(const Choose& choose) {
    CoverChoice choice{choose.gameId, false, {}};
    if (!choose.cover) {
        choice.kept = overrides_.chooseOwn(choose.gameId, choice.error);
    } else {
        const std::optional<std::string> bytes = web_.get(choose.cover->url, choice.error);
        choice.kept = bytes && overrides_.choose(choose.gameId, *bytes, choice.error);
    }
    if (choice.kept) {
        lucent::info("artwork", "{}: cover chosen ({})", choose.gameId,
                     choose.cover ? "SteamGridDB " + std::to_string(choose.cover->id) : "its own");
    } else {
        lucent::warn("artwork", "{}: choosing a cover: {}", choose.gameId, choice.error);
    }
    const std::lock_guard lock{mutex_};
    choices_.push_back(std::move(choice));
}

} // namespace opensu::artwork
