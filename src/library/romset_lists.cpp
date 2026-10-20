#include "romset_lists.hpp"

#include <algorithm>
#include <utility>

#include "lucent/log.h"

namespace opensu::library::roms {

RomsetLists::RomsetLists(const std::filesystem::path& cacheDir)
    : dats_{cacheDir.empty() ? RomsetDats{} : RomsetDats::under(cacheDir, &web_)} {
    if (dats_.enabled()) {
        worker_ = std::jthread{[this](const std::stop_token& stop) {
            run(stop);
        }};
    }
}

RomsetLists::~RomsetLists() {
    worker_.request_stop();
    wake_.notify_all();
}

RomsetFetch* RomsetLists::find(std::string_view id) {
    const auto found = std::ranges::find(fetches_, id, &RomsetFetch::id);
    return found == fetches_.end() ? nullptr : &*found;
}

void RomsetLists::want(const EmulatorEntry& entry, Clock::time_point now) {
    if (!dats_.enabled() || entry.romsets.empty()) {
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        if (const RomsetFetch* known = find(entry.id);
            known != nullptr &&
            (known->phase == RomsetFetch::Phase::Queued ||
             known->phase == RomsetFetch::Phase::Downloading ||
             (known->phase == RomsetFetch::Phase::Failed && now < known->retryAt))) {
            return;
        }
    }
    if (!dats_.due(entry, now)) {
        return;
    }
    {
        const std::lock_guard lock{mutex_};
        RomsetFetch* known = find(entry.id);
        if (known == nullptr) {
            known = &fetches_.emplace_back();
        }
        *known = RomsetFetch{.id = entry.id, .name = entry.name};
        queue_.push_back(entry);
    }
    wake_.notify_all();
}

bool RomsetLists::fetching(std::string_view id) const {
    const std::lock_guard lock{mutex_};
    const auto found = std::ranges::find(fetches_, id, &RomsetFetch::id);
    return found != fetches_.end() && (found->phase == RomsetFetch::Phase::Queued ||
                                       found->phase == RomsetFetch::Phase::Downloading);
}

std::vector<RomsetFetch> RomsetLists::fetches() const {
    const std::lock_guard lock{mutex_};
    return fetches_;
}

std::uint64_t RomsetLists::ended() const {
    const std::lock_guard lock{mutex_};
    return ended_;
}

void RomsetLists::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        EmulatorEntry entry;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            entry = std::move(queue_.front());
            queue_.pop_front();
            find(entry.id)->phase = RomsetFetch::Phase::Downloading;
        }
        fetch(entry, stop);
    }
}

void RomsetLists::fetch(const EmulatorEntry& entry, const std::stop_token& stop) {
    lucent::info("roms", "fetching {}'s romset listing", entry.name);
    const net::WebClient::Progress progress = [this, &entry, &stop](std::uint64_t received,
                                                                    std::uint64_t total) {
        const std::lock_guard lock{mutex_};
        RomsetFetch* known = find(entry.id);
        known->received = received;
        known->total = total;
        return !stop.stop_requested();
    };
    std::string error;
    const bool kept = dats_.refresh(entry, Clock::now(), error, progress);
    if (stop.stop_requested()) {
        return;
    }
    if (!error.empty()) {
        lucent::warn("roms", "{}", error);
    }
    const std::lock_guard lock{mutex_};
    RomsetFetch* known = find(entry.id);
    if (kept && error.empty()) {
        known->phase = RomsetFetch::Phase::Done;
        known->received = std::max(known->received, known->total);
        known->total = known->received;
    } else {
        known->phase = RomsetFetch::Phase::Failed;
        known->error = error.empty() ? "no listing is kept" : error;
        known->retryAt = Clock::now() + romsetRetryAfter;
    }
    ++ended_;
}

} // namespace opensu::library::roms
