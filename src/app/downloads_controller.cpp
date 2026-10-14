#include "downloads_controller.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>

#include "library/emulator_catalog.hpp"

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
    if (row.store) {
        return std::string{library::label(*row.store)};
    }
    return row.kind == DownloadKind::Core ? "Core" : "Emulator";
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

ui::DownloadsEntry entryOf(const DownloadRow& row) {
    ui::DownloadsEntry entry{.key = row.key,
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

DownloadsController::DownloadsController(ui::DownloadsPage& page, Sources sources,
                                         audio::SdlSoundPlayer& sounds, Hooks hooks)
    : page_{page}, sources_{std::move(sources)}, sounds_{sounds}, hooks_{std::move(hooks)},
      board_{sources_.titles} {
}

DownloadsController::~DownloadsController() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::vector<ui::DownloadsEntry> DownloadsController::entries() const {
    std::vector<ui::DownloadsEntry> shown;
    shown.reserve(board_.rows().size());
    for (const DownloadRow& row : board_.rows()) {
        shown.push_back(entryOf(row));
    }
    return shown;
}

void DownloadsController::open() {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    service();
    page_.open(entries());
}

void DownloadsController::close() {
    if (!page_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    page_.close();
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
    const auto nameOf = [&catalog](const std::string& id) {
        const library::roms::EmulatorEntry* entry = catalog.find(id);
        return std::pair{entry != nullptr ? entry->name : id, entry != nullptr && entry->builtIn()};
    };
    if (const std::optional<EmulatorDownloads::Progress> running = sources_.emulators.progress()) {
        const auto [name, core] = nameOf(running->id);
        look.emulator = EmulatorLook{running->id, name, core, running->received, running->total};
    }
    if (const std::optional<EmulatorDownloads::Finished> finished =
            sources_.emulators.lastFinished()) {
        using Outcome = EmulatorDownloads::Finished::Outcome;
        const auto [name, core] = nameOf(finished->id);
        look.emulatorEnded = EmulatorEnd{
            .serial = finished->serial,
            .id = finished->id,
            .name = name,
            .core = core,
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

    board_.update(look);
    if (page_.isOpen()) {
        std::vector<ui::DownloadsEntry> shown = entries();
        if (shown != page_.entries()) {
            page_.refresh(std::move(shown));
        }
    }
}

void DownloadsController::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move on a page.
        if (page_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        if (page_.step(button == gamepad::Button::Right ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A: {
        const ui::DownloadsEntry* entry = page_.focused();
        const DownloadRow* row = entry != nullptr ? board_.find(entry->key) : nullptr;
        if (row == nullptr) {
            break;
        }
        const std::vector<DownloadAction> actions = actionsFor(*row);
        if (page_.action() < actions.size()) {
            perform(*row, actions[page_.action()]);
        }
        break;
    }
    case gamepad::Button::B:
        close();
        break;
    default:
        break;
    }
}

void DownloadsController::perform(const DownloadRow& row, DownloadAction action) {
    if (action == DownloadAction::Cancel) {
        if (row.kind == DownloadKind::Game) {
            cancelInstall(row);
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
                                   .percent = percentOf(row)});
    }
    return out;
}

} // namespace opensu::app
