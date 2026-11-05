#include "save_copy_controller.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>

#include "launch/emulator_running.hpp"
#include "save_copy_screen.hpp"
#include "ui/moment_text.hpp"
#include "ui/size_text.hpp"

namespace opensu::app {
namespace {

using library::saves::EmulatorSaves;
using ui::RowKind;

constexpr std::string_view fromId = "saves.from";
constexpr std::string_view toId = "saves.to";
constexpr std::string_view copyId = "saves.copy";
constexpr std::string_view undoId = "saves.undo";
constexpr std::string_view closeId = "saves.close";
constexpr std::uintmax_t percent = 100;

/// "Eden · 2026-04-29 18:09 · 92.6 KB", or "Ryujinx · No save yet" for one holding no file of the
/// game.
std::string savesText(const EmulatorSaves& saves) {
    const std::optional<std::chrono::system_clock::time_point> modified = saves.modified();
    if (!modified) {
        return saves.emulatorName + " · No save yet";
    }
    return saves.emulatorName + " · " + ui::momentText(*modified) + " · " +
           ui::sizeText(saves.bytes());
}

std::size_t wrapped(std::size_t at, int step, std::size_t count) {
    const auto size = static_cast<long>(count);
    return static_cast<std::size_t>(((static_cast<long>(at) + step) % size + size) % size);
}

/// "45%" of what `progress` has written.
std::string progressText(const library::saves::CopyProgress& progress) {
    const std::uintmax_t total = progress.total;
    const std::uintmax_t done = std::min<std::uintmax_t>(progress.done, total);
    return std::to_string(total == 0 ? 0 : done * percent / total) + "%";
}

std::string namesOf(const std::vector<EmulatorSaves>& saves) {
    std::string names;
    for (const EmulatorSaves& one : saves) {
        names += (names.empty() ? "" : ", ") + one.emulatorName;
    }
    return names;
}

} // namespace

const gpu2d::ui::Screen* SaveCopyController::card() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const SaveCopyScreen*>(screen) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool SaveCopyController::isOpen() const {
    return card() != nullptr;
}

template <std::invocable Job> void SaveCopyController::start(Work work, Job job) {
    if (work_ != Work::None) {
        return;
    }
    work_ = work;
    progress_.done = 0;
    progress_.total = 0;
    worker_ = std::jthread{[this, job = std::move(job)] {
        Done done = job();
        const std::lock_guard lock{mutex_};
        done_ = std::move(done);
    }};
}

SaveCopyController::Found
SaveCopyController::find(const Opened& opened,
                         const library::roms::EmulatorCatalog& catalog) const {
    Found found{.gameId = opened.game.id, .saves = {}, .backups = {}};
    found.saves = library::saves::savesOf(opened.game, catalog, profiles_.runnerSaves(),
                                          opened.key.profile, opened.seat);
    for (const EmulatorSaves& saves : found.saves) {
        if (std::optional<library::saves::SaveBackup> backup =
                copier_.latest(opened.key, saves.emulatorId)) {
            found.backups.emplace(saves.emulatorId, std::move(*backup));
        }
    }
    return found;
}

void SaveCopyController::read(const library::Game& game) {
    std::optional<Opened> opened = openedFor(game);
    if (!opened || work_ != Work::None) {
        return;
    }
    reading_ = game.id;
    library::roms::EmulatorCatalog catalog = downloads_.setup().catalog;
    // Moved in: the worker's job copies nothing.
    start(Work::Reading, [this, opened = std::move(*opened), catalog = std::move(catalog)] {
        return Done{find(opened, catalog), {}, false};
    });
}

std::optional<SaveCopyController::Opened>
SaveCopyController::openedFor(const library::Game& game) const {
    const profiles::Profile active = profiles_.book().active();
    std::optional<library::saves::SaveKey> key = library::saves::keyOf(game, active.id);
    const std::optional<std::size_t> seat = profiles_.book().seatOf(active.id);
    if (!key || !seat) {
        return std::nullopt;
    }
    return Opened{.game = game, .key = std::move(*key), .seat = *seat};
}

std::string SaveCopyController::offer(const library::Game& game) const {
    if (!found_ || found_->gameId != game.id || reading_ == game.id ||
        !library::saves::copyable(found_->saves)) {
        return {};
    }
    return namesOf(found_->saves);
}

void SaveCopyController::open(const library::Game& game) {
    std::optional<Opened> opened = openedFor(game);
    if (isOpen() || !opened || offer(game).empty()) {
        return;
    }
    opened_ = std::move(opened);
    // The newest saves are the ones the player most likely wants to carry over.
    const std::vector<std::size_t> held = sources();
    from_ = *std::ranges::max_element(held, {}, [this](std::size_t at) {
        return found_->saves[at].modified();
    });
    to_ = targets().front();
    sounds_.play(audio::Effect::OpenContextMenu);
    navigator_.push(std::make_unique<SaveCopyScreen>(*this, chrome_));
}

void SaveCopyController::close() {
    const gpu2d::ui::Screen* open = card();
    if (open == nullptr) {
        return;
    }
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(*open);
}

void SaveCopyController::service() {
    std::optional<Done> done;
    {
        const std::lock_guard lock{mutex_};
        done = std::exchange(done_, std::nullopt);
    }
    if (!done) {
        return;
    }
    worker_.join();
    const Work finished = std::exchange(work_, Work::None);
    reading_.clear();
    const bool shown = found_ && opened_ && found_->gameId == opened_->game.id;
    const std::string source = shown ? found_->saves[from_].emulatorId : std::string{};
    const std::string target = shown ? found_->saves[to_].emulatorId : std::string{};
    found_ = std::move(done->found);
    if (finished != Work::Reading) {
        hooks_.say(done->said, done->error);
    }
    hooks_.changed();
    if (!opened_ || !isOpen()) {
        return;
    }
    if (found_->gameId != opened_->game.id || !library::saves::copyable(found_->saves)) {
        close();
        return;
    }
    place(source, target);
}

void SaveCopyController::place(const std::string& source, const std::string& target) {
    if (!found_) {
        return;
    }
    const std::vector<EmulatorSaves>& saves = found_->saves;
    const auto at = [&saves](const std::string& id) {
        const auto found = std::ranges::find(saves, id, &EmulatorSaves::emulatorId);
        return found == saves.end() ? saves.size()
                                    : static_cast<std::size_t>(found - saves.begin());
    };
    const std::vector<std::size_t> held = sources();
    const std::size_t was = at(source);
    from_ = std::ranges::find(held, was) != held.end() ? was : held.front();
    const std::vector<std::size_t> others = targets();
    const std::size_t aimed = at(target);
    to_ = std::ranges::find(others, aimed) != others.end() ? aimed : others.front();
}

std::vector<std::size_t> SaveCopyController::sources() const {
    std::vector<std::size_t> places;
    for (std::size_t i = 0; found_ && i < found_->saves.size(); ++i) {
        if (found_->saves[i].holdsFiles()) {
            places.push_back(i);
        }
    }
    return places;
}

std::vector<std::size_t> SaveCopyController::targets() const {
    std::vector<std::size_t> places;
    for (std::size_t i = 0; found_ && i < found_->saves.size(); ++i) {
        if (i != from_) {
            places.push_back(i);
        }
    }
    return places;
}

std::vector<ui::SettingsRow> SaveCopyController::rows() const {
    std::vector<ui::SettingsRow> rows;
    if (!opened_ || !found_ || !library::saves::copyable(found_->saves)) {
        return rows;
    }
    const std::string& title = opened_->game.title;
    const EmulatorSaves& from = found_->saves[from_];
    const EmulatorSaves& to = found_->saves[to_];
    const std::vector<std::size_t> held = sources();
    ui::Options fromOptions;
    for (const std::size_t place : held) {
        fromOptions.labels.push_back(savesText(found_->saves[place]));
    }
    fromOptions.chosen = static_cast<std::size_t>(std::ranges::find(held, from_) - held.begin());
    rows.push_back(
        ui::makeChoice(std::string{fromId}, "Copy from", "The saves to keep", fromOptions));
    ui::Options toOptions;
    const std::vector<std::size_t> others = targets();
    for (const std::size_t place : others) {
        toOptions.labels.push_back(savesText(found_->saves[place]));
    }
    toOptions.chosen = static_cast<std::size_t>(std::ranges::find(others, to_) - others.begin());
    rows.push_back(ui::makeChoice(
        std::string{toId}, "Copy to",
        to.holdsFiles() ? "The saves replaced" : "No save of the game yet", toOptions));
    if (work_ == Work::Copying || work_ == Work::Undoing) {
        rows.push_back(ui::makeRow(std::string{copyId}, RowKind::Info,
                                   work_ == Work::Copying ? "Copying saves" : "Putting saves back",
                                   to.emulatorName, progressText(progress_)));
        rows.push_back(ui::makeRow(std::string{closeId}, RowKind::Action, "Close", {}, {}));
        return rows;
    }
    ui::SettingsRow copy =
        ui::makeRow(std::string{copyId}, RowKind::Action, "Copy saves",
                    to.holdsFiles() ? to.emulatorName + "'s saves are backed up first"
                                    : "Undo copy takes them out again",
                    {});
    copy.confirm =
        from.emulatorName + "'s saves of " + title +
        (to.holdsFiles() ? " replace " + to.emulatorName + "'s. " + to.emulatorName +
                               "'s are backed up first and can be put back here."
                         : " are copied to " + to.emulatorName + ". Undo copy removes them again.");
    copy.confirmVerb = "Copy";
    rows.push_back(std::move(copy));
    if (const auto backup = found_->backups.find(to.emulatorId); backup != found_->backups.end()) {
        ui::SettingsRow undo =
            ui::makeRow(std::string{undoId}, RowKind::Action, "Undo copy",
                        "Puts back " + to.emulatorName + "'s saves from before " +
                            ui::momentText(backup->second.made),
                        {});
        undo.confirm = to.emulatorName + "'s saves of " + title +
                       " go back to how they were before " + backup->second.from +
                       "'s were copied over.";
        undo.confirmVerb = "Undo";
        rows.push_back(std::move(undo));
    }
    rows.push_back(ui::makeRow(std::string{closeId}, RowKind::Action, "Close", {}, {}));
    return rows;
}

void SaveCopyController::run(const std::string& id, int step) {
    if (id == closeId) {
        close();
        return;
    }
    if (!opened_ || !found_ || !library::saves::copyable(found_->saves) || work_ != Work::None) {
        return;
    }
    if (id == fromId) {
        const std::string target = found_->saves[to_].emulatorId;
        const std::vector<std::size_t> held = sources();
        const auto at = static_cast<std::size_t>(std::ranges::find(held, from_) - held.begin());
        from_ = held[wrapped(at, step, held.size())];
        place(found_->saves[from_].emulatorId, target);
    } else if (id == toId) {
        const std::vector<std::size_t> others = targets();
        const auto at = static_cast<std::size_t>(std::ranges::find(others, to_) - others.begin());
        to_ = others[wrapped(at, step, others.size())];
    } else if (id == copyId) {
        copy(*opened_);
    } else if (id == undoId) {
        undo(*opened_);
    }
}

std::string SaveCopyController::runningRefusal(std::initializer_list<const EmulatorSaves*> saves) {
    for (const EmulatorSaves* one : saves) {
        if (std::string refused = launch::runningRefusal(*one); !refused.empty()) {
            return refused;
        }
    }
    return {};
}

void SaveCopyController::copy(Opened opened) {
    if (!found_) {
        return;
    }
    EmulatorSaves from = found_->saves[from_];
    EmulatorSaves to = found_->saves[to_];
    library::roms::EmulatorCatalog catalog = downloads_.setup().catalog;
    start(Work::Copying, [this, opened = std::move(opened), from = std::move(from),
                          to = std::move(to), catalog = std::move(catalog)] {
        Done done;
        done.said = runningRefusal({&from, &to});
        if (done.said.empty()) {
            done.said =
                copier_.copy(opened.key, from, to, std::chrono::system_clock::now(), progress_);
        }
        done.error = !done.said.empty();
        if (!done.error) {
            done.said = from.emulatorName + "'s saves copied to " + to.emulatorName;
        }
        done.found = find(opened, catalog);
        return done;
    });
}

void SaveCopyController::undo(Opened opened) {
    if (!found_) {
        return;
    }
    EmulatorSaves to = found_->saves[to_];
    library::roms::EmulatorCatalog catalog = downloads_.setup().catalog;
    start(Work::Undoing,
          [this, opened = std::move(opened), to = std::move(to), catalog = std::move(catalog)] {
              Done done;
              done.said = runningRefusal({&to});
              if (done.said.empty()) {
                  done.said = copier_.undo(opened.key, to.emulatorId, progress_);
              }
              done.error = !done.said.empty();
              if (!done.error) {
                  done.said = to.emulatorName + "'s saves are back as they were";
              }
              done.found = find(opened, catalog);
              return done;
          });
}

} // namespace opensu::app
