#include "downloads_controller.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

#include "downloads_screen.hpp"
#include "library/emulator_catalog.hpp"
#include "library/shader_pack.hpp"

namespace opensu::app {
namespace {

constexpr int secondsPerMinute = 60;
constexpr int secondsPerHour = 3600;
constexpr double percentScale = 100.0;
constexpr double unitStep = 1000.0;

/// `12.3 MB`: decimal units, as Steam shows sizes.
std::string sizeText(std::uint64_t bytes) {
    constexpr std::array<const char*, 5> units{"B", "KB", "MB", "GB", "TB"};
    auto value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= unitStep && unit + 1 < units.size()) {
        value /= unitStep;
        ++unit;
    }
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), unit == 0 ? "%.0f %s" : "%.1f %s", value, units[unit]);
    return text.data();
}

std::string timeText(int seconds) {
    if (seconds >= secondsPerHour) {
        const int minutes = (seconds % secondsPerHour) / secondsPerMinute;
        return std::to_string(seconds / secondsPerHour) + " h " + std::to_string(minutes) +
               " min left";
    }
    if (seconds >= secondsPerMinute) {
        return std::to_string((seconds + secondsPerMinute - 1) / secondsPerMinute) + " min left";
    }
    return std::to_string(seconds) + " s left";
}

std::string sourceText(const DownloadRow& row) {
    if (row.kind == DownloadKind::Tool) {
        return "Steam tool";
    }
    if (row.store) {
        return std::string{library::label(*row.store)};
    }
    switch (row.kind) {
    case DownloadKind::Core:
        return "Core";
    case DownloadKind::ShaderPack:
        return "Shaders";
    case DownloadKind::RomsetList:
        return "Arcade";
    case DownloadKind::WineRunner:
        return "Wine runner";
    case DownloadKind::Game:
    case DownloadKind::Tool:
    case DownloadKind::Emulator:
        break;
    }
    return "Emulator";
}

/// Why a fetch failed and when it is tried again.
std::string failureText(const library::roms::RomsetFetch& fetch,
                        library::roms::RomsetLists::Clock::time_point now) {
    const auto minutes =
        std::chrono::ceil<std::chrono::minutes>(std::max(fetch.retryAt - now, {})).count();
    return fetch.error + "  ·  tried again in " + std::to_string(minutes) + " min";
}

DownloadState stateOf(library::roms::RomsetFetch::Phase phase) {
    using Phase = library::roms::RomsetFetch::Phase;
    switch (phase) {
    case Phase::Queued:
        return DownloadState::Queued;
    case Phase::Downloading:
        return DownloadState::Downloading;
    case Phase::Done:
        return DownloadState::Finished;
    case Phase::Failed:
        break;
    }
    return DownloadState::Failed;
}

std::optional<int> percentOf(const DownloadRow& row) {
    if (!row.fraction) {
        return std::nullopt;
    }
    return static_cast<int>(std::floor(std::clamp(*row.fraction, 0.0, 1.0) * percentScale));
}

void join(std::string& out, const std::string& part) {
    if (part.empty()) {
        return;
    }
    out += out.empty() ? part : "  ·  " + part;
}

} // namespace

DownloadsEntry entryOf(const DownloadRow& row) {
    DownloadsEntry entry{.key = row.key,
                         .title = row.title,
                         .source = sourceText(row),
                         .state = row.update && row.state == DownloadState::Downloading
                                      ? std::string{"Updating"}
                                      : std::string{label(row.state)},
                         .fraction = row.fraction,
                         .failed = row.state == DownloadState::Failed,
                         .ended = ended(row.state)};
    if (row.state == DownloadState::Failed || row.state == DownloadState::Cancelled) {
        entry.figures = row.state == DownloadState::Failed ? row.detail : std::string{};
        entry.fraction.reset();
    } else {
        if (const std::optional<int> percent = percentOf(row);
            percent && row.state != DownloadState::Finished) {
            join(entry.figures, std::to_string(*percent) + "%");
        }
        if (row.bytesTotal > 0) {
            join(entry.figures, row.state == DownloadState::Finished
                                    ? sizeText(row.bytesTotal)
                                    : sizeText(row.bytesDone) + " of " + sizeText(row.bytesTotal));
        }
        if (row.bytesPerSecond && row.state == DownloadState::Downloading) {
            join(entry.figures, sizeText(*row.bytesPerSecond) + "/s");
        }
        if (row.secondsLeft && !ended(row.state)) {
            join(entry.figures, timeText(*row.secondsLeft));
        }
        // An install openSU runs says what it waits on before it measures anything.
        if (entry.figures.empty() && !ended(row.state)) {
            entry.figures = row.detail;
        }
    }
    for (const DownloadAction action : actionsFor(row)) {
        entry.actions.emplace_back(label(action));
    }
    return entry;
}

DownloadsController::DownloadsController(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome,
                                         Sources sources, audio::SdlSoundPlayer& sounds,
                                         Hooks hooks)
    : navigator_{navigator}, chrome_{chrome}, sources_{std::move(sources)}, sounds_{sounds},
      hooks_{std::move(hooks)}, board_{sources_.titles} {
}

DownloadsController::~DownloadsController() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool DownloadsController::isOpen() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const DownloadsScreen*>(screen) != nullptr;
    });
}

void DownloadsController::open() {
    if (isOpen()) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    service();
    navigator_.push(std::make_unique<DownloadsScreen>(*this, chrome_));
}

void DownloadsController::close() {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const DownloadsScreen*>(screen) != nullptr;
    });
    if (found == open.end()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(**found);
}

void DownloadsController::service() {
    DownloadsBoard::Sources look;
    look.steamReady = sources_.steamReady();
    if (look.steamReady) {
        look.steam = sources_.steamQueue();
    }
    if (std::optional<Installs::Status> install = sources_.installs.status()) {
        look.install = std::move(*install);
    }
    const library::roms::EmulatorCatalog& catalog = sources_.emulators.setup().catalog;
    const auto kindOf = [&catalog](const std::string& id) {
        if (id == library::roms::shaderPackId) {
            return DownloadKind::ShaderPack;
        }
        const library::roms::EmulatorEntry* entry = catalog.find(id);
        return entry != nullptr && entry->builtIn() ? DownloadKind::Core : DownloadKind::Emulator;
    };
    if (const std::optional<EmulatorDownloads::Progress> running = sources_.emulators.progress()) {
        look.emulator = EmulatorLook{running->id, sources_.emulators.nameOf(running->id),
                                     kindOf(running->id), running->received, running->total};
    }
    if (const std::optional<EmulatorDownloads::Finished> finished =
            sources_.emulators.lastFinished()) {
        using Outcome = EmulatorDownloads::Finished::Outcome;
        look.emulatorEnded = EmulatorEnd{
            .serial = finished->serial,
            .id = finished->id,
            .name = sources_.emulators.nameOf(finished->id),
            .kind = kindOf(finished->id),
            .state = finished->outcome == Outcome::Installed   ? DownloadState::Finished
                     : finished->outcome == Outcome::Cancelled ? DownloadState::Cancelled
                                                               : DownloadState::Failed,
            .text = finished->text};
    }

    if (const std::optional<WineRunners::Progress> running = sources_.runners.progress()) {
        // Past the transfer the row reads as installing: checking the digest, unpacking.
        const bool moving = running->stage == library::ArchiveStage::Downloading;
        look.runner = EmulatorLook{running->id, running->name, DownloadKind::WineRunner,
                                   moving ? running->received : running->total, running->total};
    }
    if (const std::optional<WineRunners::Finished> finished = sources_.runners.lastFinished()) {
        using Outcome = WineRunners::Finished::Outcome;
        look.runnerEnded = EmulatorEnd{
            .serial = finished->serial,
            .id = finished->id,
            .name = finished->name,
            .kind = DownloadKind::WineRunner,
            .state = finished->outcome == Outcome::Installed   ? DownloadState::Finished
                     : finished->outcome == Outcome::Cancelled ? DownloadState::Cancelled
                                                               : DownloadState::Failed,
            .text = finished->text};
    }

    std::optional<SteamAnswer> answer;
    {
        const std::lock_guard lock{mutex_};
        answer = std::exchange(answer_, std::nullopt);
    }
    if (answer) {
        if (worker_.joinable()) {
            worker_.join();
        }
        asking_ = false;
        if (!answer->refusal.empty()) {
            hooks_.say(answer->refusal, true);
        } else if (answer->action == DownloadAction::Remove) {
            board_.removed(answer->key);
        }
    }

    look.romsetLists = lookAtRomsetLists();
    board_.update(look);
    shown_.clear();
    for (const DownloadRow& row : board_.rows()) {
        shown_.push_back(entryOf(row));
    }
}

std::vector<RomsetListLook> DownloadsController::lookAtRomsetLists() {
    using Phase = library::roms::RomsetFetch::Phase;
    const library::roms::RomsetLists& lists = sources_.romsetLists;
    const std::uint64_t endedNow = lists.ended();
    std::vector<library::roms::RomsetFetch> fetches = lists.fetches();
    if (endedNow != romsetListsEnded_) {
        romsetListsEnded_ = endedNow;
        for (const library::roms::RomsetFetch& fetch : fetches) {
            const auto before =
                std::ranges::find(romsetFetches_, fetch.id, &library::roms::RomsetFetch::id);
            const bool wasEnded = before != romsetFetches_.end() && before->phase == fetch.phase;
            if (wasEnded || (fetch.phase != Phase::Done && fetch.phase != Phase::Failed)) {
                continue;
            }
            hooks_.say(fetch.phase == Phase::Done ? "Romset list of " + fetch.name + " downloaded"
                                                  : fetch.error,
                       fetch.phase == Phase::Failed);
        }
        hooks_.romsetListsEnded();
    }
    const auto now = library::roms::RomsetLists::Clock::now();
    std::vector<RomsetListLook> looks;
    looks.reserve(fetches.size());
    for (const library::roms::RomsetFetch& fetch : fetches) {
        looks.push_back(RomsetListLook{
            .id = fetch.id,
            .name = fetch.name,
            .state = stateOf(fetch.phase),
            .received = fetch.received,
            .total = fetch.total,
            .detail = fetch.phase == Phase::Failed ? failureText(fetch, now) : std::string{}});
    }
    romsetFetches_ = std::move(fetches);
    return looks;
}

void DownloadsController::press(const std::string& key, std::size_t action) {
    const DownloadRow* row = board_.find(key);
    if (row == nullptr) {
        return;
    }
    const std::vector<DownloadAction> actions = actionsFor(*row);
    if (action < actions.size()) {
        perform(*row, actions[action]);
    }
}

void DownloadsController::perform(const DownloadRow& row, DownloadAction action) {
    if (action == DownloadAction::Cancel) {
        if (row.store) {
            cancelInstall(row);
        } else if (row.kind == DownloadKind::WineRunner) {
            sources_.runners.cancel();
        } else {
            sources_.emulators.cancel(row.id);
        }
        return;
    }
    askSteam(row, action);
    // Steam's installer is following this download for openSU; it goes with it.
    if (action == DownloadAction::Remove) {
        cancelInstall(row);
    }
}

void DownloadsController::cancelInstall(const DownloadRow& row) {
    const std::optional<Installs::Status> install = sources_.installs.status();
    if (install && row.store == install->store && install->job.appId == row.id &&
        (install->job.phase == InstallJob::Phase::Running ||
         install->job.phase == InstallJob::Phase::Licence)) {
        sources_.installs.cancel();
    }
}

void DownloadsController::askSteam(const DownloadRow& row, DownloadAction action) {
    if (asking_) {
        hooks_.say("Steam is still answering", true);
        return;
    }
    asking_ = true;
    steam::Download download{.appId = row.id,
                             .paused = row.state == DownloadState::Paused,
                             .queueIndex = row.queueIndex};
    worker_ = std::jthread{[this, key = row.key, action, download = std::move(download)] {
        steam::DownloadControl& steam = sources_.steam;
        std::string refusal;
        switch (action) {
        case DownloadAction::Pause:
            refusal = steam.pause();
            break;
        case DownloadAction::Resume:
            refusal = steam.resume();
            break;
        case DownloadAction::MoveToTop:
            refusal = steam.moveToTop(download.appId);
            break;
        case DownloadAction::MoveUp:
            refusal = steam.setQueueIndex(download.appId, download.queueIndex - 1);
            break;
        case DownloadAction::MoveDown:
            refusal = steam.setQueueIndex(download.appId, download.queueIndex + 1);
            break;
        case DownloadAction::Remove:
            refusal = steam.remove(download);
            break;
        case DownloadAction::Cancel:
            break;
        }
        const std::lock_guard lock{mutex_};
        answer_ = SteamAnswer{key, action, std::move(refusal)};
    }};
}

std::vector<DownloadLine> DownloadsController::lines() const {
    std::vector<DownloadLine> out;
    for (const DownloadRow& row : board_.rows()) {
        out.push_back(DownloadLine{.key = row.key,
                                   .title = row.title,
                                   .source = sourceText(row),
                                   .state = std::string{spelling(row.state)},
                                   .percent = percentOf(row),
                                   .figures = entryOf(row).figures});
    }
    return out;
}

} // namespace opensu::app
