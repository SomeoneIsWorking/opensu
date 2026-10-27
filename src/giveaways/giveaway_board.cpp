#include "giveaway_board.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "offer_feed.hpp"

namespace opensu::giveaways {
namespace {

namespace fs = std::filesystem;

constexpr const char* cacheName = "offers.json";
constexpr const char* artDir = "art";

Seconds unixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::chrono::system_clock::time_point timeOf(Seconds seconds) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
}

} // namespace

GiveawayBoard::GiveawayBoard(const fs::path& cacheDir) {
    if (cacheDir.empty()) {
        return;
    }
    root_ = cacheDir / "giveaways";
    std::ifstream file{root_ / cacheName};
    if (file) {
        const std::string text{std::istreambuf_iterator<char>{file}, {}};
        if (std::optional<Cached> cached = fromJson(text)) {
            board_.offers = std::move(cached->offers);
            board_.fetched = cached->fetched;
        } else {
            lucent::warn("giveaways", "{} does not read; the stores are asked again",
                         (root_ / cacheName).string());
        }
    }
    for (const Offer& offer : board_.offers) {
        if (const fs::path art = artFile(offer); fs::exists(art)) {
            board_.art.emplace(key(offer), art);
        }
    }
    worker_ = std::jthread{[this](const std::stop_token& stop) {
        run(stop);
    }};
}

GiveawayBoard::~GiveawayBoard() {
    worker_.request_stop();
    wake_.notify_all();
}

void GiveawayBoard::refresh() {
    {
        const std::lock_guard lock{mutex_};
        if (board_.reading) {
            return;
        }
        asked_ = true;
    }
    wake_.notify_all();
}

Board GiveawayBoard::board() const {
    const std::lock_guard lock{mutex_};
    return board_;
}

std::uint64_t GiveawayBoard::generation() const {
    const std::lock_guard lock{mutex_};
    return generation_;
}

fs::path GiveawayBoard::artFile(const Offer& offer) const {
    std::string name = key(offer);
    std::ranges::replace_if(
        name,
        [](char c) {
            return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                     c == '-' || c == '_');
        },
        '_');
    return root_ / artDir / name;
}

void GiveawayBoard::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        bool due = false;
        std::chrono::system_clock::time_point next;
        {
            const std::lock_guard lock{mutex_};
            next = std::max(timeOf(board_.fetched) + refreshEvery, retryAt_);
            due = asked_ || std::chrono::system_clock::now() >= next;
            asked_ = false;
        }
        if (due) {
            read(stop);
        }
        fetchArt(stop);
        std::unique_lock lock{mutex_};
        next = std::max(timeOf(board_.fetched) + refreshEvery, retryAt_);
        static_cast<void>(wake_.wait_until(lock, stop, next, [this] {
            return asked_;
        }));
    }
}

void GiveawayBoard::read(const std::stop_token& stop) {
    {
        const std::lock_guard lock{mutex_};
        board_.reading = true;
        ++generation_;
    }
    const std::vector<StoreRead> reads = readStores(web_, stop);
    if (stop.stop_requested()) {
        return;
    }
    const std::lock_guard lock{mutex_};
    board_.reading = false;
    ++generation_;
    std::vector<Offer> offers;
    std::vector<std::string> failures;
    for (const StoreRead& read : reads) {
        if (read.error.empty()) {
            std::ranges::copy(read.offers, std::back_inserter(offers));
            continue;
        }
        failures.push_back(std::string{library::label(read.store)} + ": " + read.error);
        std::ranges::copy_if(board_.offers, std::back_inserter(offers),
                             [&read](const Offer& offer) {
                                 return offer.store == read.store;
                             });
    }
    board_.failures = std::move(failures);
    if (board_.failures.size() == reads.size()) {
        retryAt_ = std::chrono::system_clock::now() + retryAfter;
        return;
    }
    board_.offers = std::move(offers);
    board_.fetched = unixNow();
    std::string error;
    if (!fileio::writeWhole(root_ / cacheName, toJson(board_.offers, board_.fetched), error)) {
        lucent::warn("giveaways", "the offers are not kept: {}", error);
    }
}

void GiveawayBoard::fetchArt(const std::stop_token& stop) {
    std::vector<Offer> missing;
    std::set<fs::path> kept;
    {
        const std::lock_guard lock{mutex_};
        for (const Offer& offer : board_.offers) {
            kept.insert(artFile(offer));
            if (!offer.artUrl.empty() && !board_.art.contains(key(offer))) {
                missing.push_back(offer);
            }
        }
    }
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator{root_ / artDir, error}) {
        if (!kept.contains(entry.path())) {
            fs::remove(entry.path(), error);
        }
    }
    fs::create_directories(root_ / artDir, error);
    for (const Offer& offer : missing) {
        if (stop.stop_requested()) {
            return;
        }
        const fs::path file = artFile(offer);
        const fs::path part = fs::path{file}.concat(".part");
        std::string why;
        const bool done = web_.download(
            offer.artUrl, part,
            [&stop](std::uint64_t, std::uint64_t) {
                return !stop.stop_requested();
            },
            why);
        if (!done) {
            fs::remove(part, error);
            lucent::warn("giveaways", "art of {}: {}", offer.title, why);
            continue;
        }
        fs::rename(part, file, error);
        if (error) {
            lucent::warn("giveaways", "art of {}: {}", offer.title, error.message());
            continue;
        }
        const std::lock_guard lock{mutex_};
        board_.art.insert_or_assign(key(offer), file);
        ++generation_;
    }
}

} // namespace opensu::giveaways
