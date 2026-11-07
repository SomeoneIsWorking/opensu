#include "remote_cover_offers.hpp"

#include <chrono>
#include <utility>

#include "lucent/log.h"

namespace opensu::app {
namespace {

using Status = artwork::SteamGridDb::Covers::Status;

constexpr std::chrono::seconds pollEvery{1};
// Past this the host is taken to have stopped; a whole cover downloads well within it.
constexpr int mostPolls = 120;

Status statusOf(const std::string& word) {
    if (word == lan::wire::coversDisconnected) {
        return Status::Disconnected;
    }
    if (word == lan::wire::coversRefused) {
        return Status::Refused;
    }
    if (word == lan::wire::coversUnreachable) {
        return Status::Unreachable;
    }
    return Status::Listed;
}

} // namespace

void RemoteCoverOffers::list(const library::Game& game) {
    std::uint64_t generation = 0;
    {
        const std::lock_guard lock{mutex_};
        hosted_ = Hosted{game.hostId, game.sourceId};
        generation = ++generation_;
        listing_.reset();
    }
    enqueue(List{Hosted{game.hostId, game.sourceId}, game.id, generation});
}

void RemoteCoverOffers::choose(const std::string& gameId, const artwork::SteamGridDbCover& cover) {
    enqueue(Choose{hosted_, gameId, cover.id});
}

void RemoteCoverOffers::chooseOwn(const std::string& gameId) {
    enqueue(Choose{hosted_, gameId, std::nullopt});
}

std::optional<artwork::CoverListing> RemoteCoverOffers::takeListing() {
    const std::lock_guard lock{mutex_};
    return std::exchange(listing_, std::nullopt);
}

std::vector<artwork::CoverChoice> RemoteCoverOffers::takeChoices() {
    const std::lock_guard lock{mutex_};
    return std::exchange(choices_, {});
}

void RemoteCoverOffers::enqueue(Job job) {
    {
        const std::lock_guard lock{mutex_};
        jobs_.push_back(std::move(job));
        if (draining_) {
            wake_.notify_all();
            return;
        }
        draining_ = true;
    }
    const bool asked = ask_([this](const lan::LanNode& node, const std::stop_token& stop) {
        drain(node, stop);
    });
    if (asked) {
        return;
    }
    const std::lock_guard lock{mutex_};
    draining_ = false;
    for (const Job& left : std::exchange(jobs_, {})) {
        if (const auto* choose = std::get_if<Choose>(&left)) {
            choices_.push_back({choose->gameId, false, "LAN is off"});
        } else if (const auto* list = std::get_if<List>(&left);
                   list != nullptr && list->generation == generation_) {
            listing_ = artwork::CoverListing{.gameId = list->gameId,
                                             .status = Status::Unreachable,
                                             .error = "LAN is off",
                                             .complete = true};
        }
    }
}

void RemoteCoverOffers::drain(const lan::LanNode& node, const std::stop_token& stop) {
    while (true) {
        Job job;
        {
            const std::lock_guard lock{mutex_};
            if (jobs_.empty() || stop.stop_requested()) {
                jobs_.clear();
                draining_ = false;
                return;
            }
            job = std::move(jobs_.front());
            jobs_.erase(jobs_.begin());
        }
        std::visit(
            [this, &node, &stop](const auto& work) {
                perform(node, stop, work);
            },
            job);
    }
}

bool RemoteCoverOffers::pause(const std::stop_token& stop) {
    std::unique_lock lock{mutex_};
    const bool waiting = wake_.wait_for(lock, stop, pollEvery, [this] {
        return !jobs_.empty();
    });
    return !waiting && !stop.stop_requested();
}

void RemoteCoverOffers::publish(artwork::CoverListing listing, std::uint64_t generation) {
    const std::lock_guard lock{mutex_};
    if (generation == generation_) {
        listing_ = std::move(listing);
    }
}

void RemoteCoverOffers::finish(artwork::CoverChoice choice) {
    const std::lock_guard lock{mutex_};
    choices_.push_back(std::move(choice));
}

void RemoteCoverOffers::perform(const lan::LanNode& node, const std::stop_token& stop,
                                const List& list) {
    bool fresh = true;
    for (int poll = 0; poll < mostPolls; ++poll) {
        std::string error;
        const std::optional<lan::wire::CoverOffersAnswer> answer = node.coverOffers(
            stop, list.hosted.hostId, lan::wire::CoverAsk{list.hosted.game, fresh}, error);
        fresh = false;
        if (stop.stop_requested()) {
            return;
        }
        if (!answer) {
            lucent::warn("artwork", "{}: asking its host for covers: {}", list.gameId, error);
            publish({.gameId = list.gameId,
                     .status = Status::Unreachable,
                     .error = error,
                     .complete = true},
                    list.generation);
            return;
        }
        if (answer->status != lan::wire::coversAsking) {
            artwork::CoverListing listing{.gameId = list.gameId,
                                          .status = statusOf(answer->status),
                                          .complete = answer->complete};
            for (const lan::wire::OfferedCover& offer : answer->offers) {
                artwork::CoverOffer shown;
                shown.cover.id = offer.id;
                shown.cover.width = offer.width;
                shown.cover.height = offer.height;
                if (!offer.thumb.empty()) {
                    shown.thumb = node.offeredCover(stop, list.hosted.hostId, offer.thumb);
                }
                listing.offers.push_back(std::move(shown));
            }
            if (!answer->own.empty()) {
                listing.own = node.offeredCover(stop, list.hosted.hostId, answer->own);
            }
            publish(std::move(listing), list.generation);
            if (answer->complete) {
                return;
            }
        }
        if (!pause(stop)) {
            return;
        }
    }
    publish({.gameId = list.gameId,
             .status = Status::Unreachable,
             .error = "the host stopped answering",
             .complete = true},
            list.generation);
}

void RemoteCoverOffers::perform(const lan::LanNode& node, const std::stop_token& stop,
                                const Choose& choose) {
    std::string error;
    if (!node.pickCover(stop, choose.hosted.hostId,
                        lan::wire::CoverPick{choose.hosted.game, choose.cover}, error)) {
        finish({choose.gameId, false, error});
        return;
    }
    for (int poll = 0; poll < mostPolls && !stop.stop_requested(); ++poll) {
        const std::optional<lan::wire::CoverOffersAnswer> answer = node.coverOffers(
            stop, choose.hosted.hostId, lan::wire::CoverAsk{choose.hosted.game, false}, error);
        if (!answer) {
            finish({choose.gameId, false, error});
            return;
        }
        if (answer->choice == lan::wire::coverKept) {
            finish({choose.gameId, true, {}});
            return;
        }
        if (answer->choice != lan::wire::coversAsking) {
            finish({choose.gameId, false, answer->choice});
            return;
        }
        std::unique_lock lock{mutex_};
        wake_.wait_for(lock, stop, pollEvery, [] {
            return false;
        });
    }
    finish({choose.gameId, false, "the host stopped answering"});
}

} // namespace opensu::app
