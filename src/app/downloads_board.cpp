#include "downloads_board.hpp"

#include <algorithm>
#include <array>
#include <tuple>

namespace opensu::app {
namespace {

using Phase = InstallJob::Phase;

/// Steam's update states that move bytes (its downloads page shows the rate for these); "None"
/// and "Starting" come before the first byte.
constexpr std::array<std::string_view, 6> transferStates{"Downloading", "Updating", "Transferring",
                                                         "Preloading",  "Starting", "None"};

std::string keyOf(library::Source store, const std::string& id) {
    switch (store) {
    case library::Source::Steam:
        return "steam:" + id;
    case library::Source::Epic:
        return "epic:" + id;
    case library::Source::Gog:
        return "gog:" + id;
    case library::Source::Rom:
        break;
    }
    return "rom:" + id;
}

std::string emulatorKey(const std::string& id) {
    return "emulator:" + id;
}

std::string romsetListKey(const std::string& id) {
    return "romsets:" + id;
}

DownloadState steamState(const steam::Download& download) {
    if (download.active && !download.paused) {
        const bool moving =
            download.updateState.empty() ||
            std::ranges::find(transferStates, download.updateState) != transferStates.end();
        return moving ? DownloadState::Downloading : DownloadState::Installing;
    }
    if (download.paused || download.queueIndex < 0) {
        return DownloadState::Paused;
    }
    return DownloadState::Queued;
}

/// Where a row sorts among the live ones: what moves now, then what waits on the player, then the
/// queue.
int group(DownloadState state) {
    switch (state) {
    case DownloadState::Downloading:
    case DownloadState::Installing:
    case DownloadState::Uninstalling:
        return 0;
    case DownloadState::Paused:
        return 1;
    case DownloadState::Queued:
        return 2;
    case DownloadState::Finished:
    case DownloadState::Failed:
    case DownloadState::Cancelled:
    case DownloadState::Uninstalled:
        break;
    }
    return 3;
}

} // namespace

bool ended(DownloadState state) noexcept {
    return state == DownloadState::Finished || state == DownloadState::Failed ||
           state == DownloadState::Cancelled || state == DownloadState::Uninstalled;
}

std::vector<DownloadAction> actionsFor(const DownloadRow& row) {
    // An uninstall stopped half way leaves half a game, so it has no Cancel; a romset list is
    // fetched for the library, not on the player's request.
    if (ended(row.state) || row.state == DownloadState::Uninstalling ||
        row.kind == DownloadKind::RomsetList) {
        return {};
    }
    if (row.store != library::Source::Steam || !row.inSteamQueue) {
        // An install openSU runs (Steam's before its download is queued) or an emulator download.
        return {DownloadAction::Cancel};
    }
    switch (row.state) {
    case DownloadState::Downloading:
    case DownloadState::Installing:
        return {DownloadAction::Pause};
    case DownloadState::Paused:
        if (row.queueIndex == 0) {
            return {DownloadAction::Resume, DownloadAction::Remove};
        }
        return {DownloadAction::MoveToTop, DownloadAction::Remove};
    case DownloadState::Queued: {
        std::vector<DownloadAction> actions{DownloadAction::MoveToTop};
        // Place 1 moves up by going to the top; the head itself is the active download.
        if (row.queueIndex >= 2) {
            actions.push_back(DownloadAction::MoveUp);
        }
        if (!row.lastInQueue) {
            actions.push_back(DownloadAction::MoveDown);
        }
        actions.push_back(DownloadAction::Remove);
        return actions;
    }
    case DownloadState::Finished:
    case DownloadState::Failed:
    case DownloadState::Cancelled:
    case DownloadState::Uninstalling:
    case DownloadState::Uninstalled:
        break;
    }
    return {};
}

std::string_view label(DownloadAction action) noexcept {
    switch (action) {
    case DownloadAction::Pause:
        return "Pause";
    case DownloadAction::Resume:
        return "Resume";
    case DownloadAction::MoveToTop:
        return "Download now";
    case DownloadAction::MoveUp:
        return "Move up";
    case DownloadAction::MoveDown:
        return "Move down";
    case DownloadAction::Remove:
        return "Remove";
    case DownloadAction::Cancel:
        return "Cancel";
    }
    return "";
}

std::string_view label(DownloadState state) noexcept {
    switch (state) {
    case DownloadState::Queued:
        return "Queued";
    case DownloadState::Downloading:
        return "Downloading";
    case DownloadState::Paused:
        return "Paused";
    case DownloadState::Installing:
        return "Installing";
    case DownloadState::Finished:
        return "Finished";
    case DownloadState::Failed:
        return "Failed";
    case DownloadState::Cancelled:
        return "Cancelled";
    case DownloadState::Uninstalling:
        return "Uninstalling";
    case DownloadState::Uninstalled:
        return "Uninstalled";
    }
    return "";
}

std::string_view spelling(DownloadState state) noexcept {
    switch (state) {
    case DownloadState::Queued:
        return "queued";
    case DownloadState::Downloading:
        return "downloading";
    case DownloadState::Paused:
        return "paused";
    case DownloadState::Installing:
        return "installing";
    case DownloadState::Finished:
        return "finished";
    case DownloadState::Failed:
        return "failed";
    case DownloadState::Cancelled:
        return "cancelled";
    case DownloadState::Uninstalling:
        return "uninstalling";
    case DownloadState::Uninstalled:
        return "uninstalled";
    }
    return "";
}

const DownloadRow* DownloadsBoard::find(std::string_view key) const {
    const auto found = std::ranges::find(rows_, key, &DownloadRow::key);
    return found == rows_.end() ? nullptr : &*found;
}

void DownloadsBoard::removed(const std::string& key) {
    entries_.erase(key);
    removed_.insert(key);
    order();
}

DownloadsBoard::Entry& DownloadsBoard::entry(const std::string& key) {
    const auto [at, made] = entries_.try_emplace(key);
    if (made) {
        at->second.row.key = key;
        at->second.seen = tick_;
    }
    return at->second;
}

void DownloadsBoard::update(const Sources& sources) {
    ++tick_;
    std::set<std::string> live;
    if (sources.install) {
        applyInstall(*sources.install, live);
    }
    if (sources.steamReady) {
        applySteam(sources.steam, live);
    }
    applyEmulator(sources, live);
    applyRomsetLists(sources.romsetLists, live);
    settle(live, sources.steamReady);
    order();
}

void DownloadsBoard::applyInstall(const Installs::Status& install, std::set<std::string>& live) {
    const InstallJob::Status& job = install.job;
    const bool removing = install.work == Installs::Work::Uninstall;
    const std::string key = keyOf(install.store, job.appId);
    const bool running = job.phase == Phase::Running || job.phase == Phase::Licence;
    if (install.store == library::Source::Steam && install.work == Installs::Work::Update &&
        job.phase == Phase::Done) {
        // The update is on Steam's queue now, and Steam's row carries it.
        if (const auto found = entries_.find(key);
            found != entries_.end() && !found->second.row.inSteamQueue) {
            entries_.erase(found);
        }
        return;
    }
    if (!running) {
        const auto found = entries_.find(key);
        if (found != entries_.end() && found->second.endSerial == job.serial) {
            return;
        }
    }
    Entry& at = entry(key);
    DownloadRow& row = at.row;
    row.store = install.store;
    row.kind = DownloadKind::Game;
    row.id = job.appId;
    row.title = job.title;
    row.fraction = job.fraction;
    row.update = install.work == Installs::Work::Update;
    row.detail = job.phase == Phase::Failed ? job.failure : job.line;
    row.inSteamQueue = false;
    row.queueIndex = -1;
    switch (job.phase) {
    case Phase::Running:
    case Phase::Licence:
        if (removing) {
            row.state = DownloadState::Uninstalling;
            row.fraction.reset();
        } else {
            row.state = job.fraction ? DownloadState::Downloading : DownloadState::Queued;
        }
        at.live = true;
        live.insert(key);
        return;
    case Phase::Done:
        row.state = removing ? DownloadState::Uninstalled : DownloadState::Finished;
        row.fraction = removing ? std::nullopt : std::optional{1.0};
        break;
    case Phase::Failed:
        row.state = DownloadState::Failed;
        break;
    case Phase::Cancelled:
        row.state = DownloadState::Cancelled;
        break;
    }
    at.live = false;
    at.endSerial = job.serial;
    at.endedAt = tick_;
}

void DownloadsBoard::applySteam(const std::vector<steam::Download>& queue,
                                std::set<std::string>& live) {
    int last = -1;
    for (const steam::Download& download : queue) {
        last = std::max(last, download.queueIndex);
    }
    std::set<std::string, std::less<>> listed;
    for (const steam::Download& download : queue) {
        const std::string key = keyOf(library::Source::Steam, download.appId);
        listed.insert(key);
        if (removed_.contains(key)) {
            continue;
        }
        Entry& at = entry(key);
        DownloadRow& row = at.row;
        row.store = library::Source::Steam;
        row.kind = DownloadKind::Game;
        row.id = download.appId;
        if (row.title.empty()) {
            row.title = titles_(library::Source::Steam, download.appId);
        }
        if (row.title.empty()) {
            row.title = "Steam app " + download.appId;
        }
        row.state = steamState(download);
        row.update = download.active && !download.installing;
        row.fraction = download.progress;
        row.bytesDone = download.bytesDone;
        row.bytesTotal = download.bytesTotal;
        row.bytesPerSecond = download.bytesPerSecond;
        row.secondsLeft = download.secondsLeft;
        row.detail.clear();
        row.inSteamQueue = true;
        row.queueIndex = download.queueIndex;
        row.lastInQueue = download.queueIndex >= last;
        at.live = true;
        live.insert(key);
    }
    // A removal is over once Steam no longer lists it.
    std::erase_if(removed_, [&listed](const std::string& key) {
        return !listed.contains(key);
    });
}

void DownloadsBoard::applyEmulator(const Sources& sources, std::set<std::string>& live) {
    if (const std::optional<EmulatorLook>& running = sources.emulator) {
        Entry& at = entry(emulatorKey(running->id));
        DownloadRow& row = at.row;
        row.kind = running->core ? DownloadKind::Core : DownloadKind::Emulator;
        row.id = running->id;
        row.title = running->name;
        row.bytesDone = running->received;
        row.bytesTotal = running->total;
        row.fraction = running->total > 0 ? std::optional{static_cast<double>(running->received) /
                                                          static_cast<double>(running->total)}
                                          : std::nullopt;
        row.state = running->total > 0 && running->received >= running->total
                        ? DownloadState::Installing
                        : DownloadState::Downloading;
        row.detail.clear();
        at.live = true;
        live.insert(row.key);
    }
    const std::optional<EmulatorEnd>& end = sources.emulatorEnded;
    if (!end || live.contains(emulatorKey(end->id))) {
        return;
    }
    const auto found = entries_.find(emulatorKey(end->id));
    if (found != entries_.end() && found->second.endSerial == end->serial) {
        return;
    }
    Entry& at = entry(emulatorKey(end->id));
    at.row.kind = end->core ? DownloadKind::Core : DownloadKind::Emulator;
    at.row.id = end->id;
    at.row.title = end->name;
    at.row.state = end->state;
    at.row.detail = end->text;
    if (end->state == DownloadState::Finished) {
        at.row.fraction = 1.0;
    }
    at.live = false;
    at.endSerial = end->serial;
    at.endedAt = tick_;
}

void DownloadsBoard::applyRomsetLists(const std::vector<RomsetListLook>& lists,
                                      std::set<std::string>& live) {
    for (std::size_t place = 0; place < lists.size(); ++place) {
        const RomsetListLook& list = lists[place];
        Entry& at = entry(romsetListKey(list.id));
        DownloadRow& row = at.row;
        const bool wasLive = at.live || at.endedAt == 0;
        row.kind = DownloadKind::RomsetList;
        row.id = list.id;
        row.title = "Romset list · " + list.name;
        row.bytesDone = list.received;
        row.bytesTotal = list.total;
        row.fraction = list.total > 0 ? std::optional{static_cast<double>(list.received) /
                                                      static_cast<double>(list.total)}
                                      : std::nullopt;
        row.state = list.state == DownloadState::Downloading && list.total > 0 &&
                            list.received >= list.total
                        ? DownloadState::Installing
                        : list.state;
        row.detail = list.detail;
        // The lists wait in the order they are fetched in.
        row.queueIndex = static_cast<int>(place);
        at.live = !ended(row.state);
        if (at.live) {
            live.insert(row.key);
        } else if (wasLive) {
            at.endedAt = tick_;
        }
    }
}

void DownloadsBoard::settle(const std::set<std::string>& live, bool steamReady) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        Entry& at = it->second;
        if (!at.live || live.contains(it->first) || !at.row.inSteamQueue) {
            ++it;
            continue;
        }
        if (!steamReady) {
            // Steam cannot be asked: nothing is known of its downloads.
            it = entries_.erase(it);
            continue;
        }
        // Steam took it off its queue: done.
        at.live = false;
        at.row.state = DownloadState::Finished;
        at.row.fraction = 1.0;
        at.row.bytesPerSecond.reset();
        at.row.secondsLeft.reset();
        at.endedAt = tick_;
        ++it;
    }
}

void DownloadsBoard::order() {
    std::vector<const Entry*> sorted;
    sorted.reserve(entries_.size());
    for (const auto& [key, at] : entries_) {
        sorted.push_back(&at);
    }
    const auto rank = [](const Entry* at) {
        const int place = group(at->row.state);
        // Ended rows: the latest first.
        const std::uint64_t when =
            place == 3 ? ~at->endedAt : static_cast<std::uint64_t>(at->row.queueIndex + 1);
        return std::tuple{place, when, at->seen, std::string_view{at->row.key}};
    };
    std::ranges::sort(sorted, [&rank](const Entry* a, const Entry* b) {
        return rank(a) < rank(b);
    });
    rows_.clear();
    for (const Entry* at : sorted) {
        rows_.push_back(at->row);
    }
}

} // namespace opensu::app
