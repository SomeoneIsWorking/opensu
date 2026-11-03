#include "save_copy_controller.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>

#include "clock/local_time.hpp"
#include "save_copy_screen.hpp"
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

/// A moment as the card shows it: "2026-04-29 18:09", local time.
std::string momentText(std::chrono::system_clock::time_point at) {
    const std::tm parts = clock::localTime(std::chrono::system_clock::to_time_t(at));
    std::string text(32, '\0');
    text.resize(std::strftime(text.data(), text.size(), "%Y-%m-%d %H:%M", &parts));
    return text;
}

/// "Eden · 2026-04-29 18:09 · 92.6 KB", or "Ryujinx · Empty" for saves holding no file.
std::string savesText(const EmulatorSaves& saves) {
    const std::optional<std::chrono::system_clock::time_point> modified = saves.modified();
    if (!modified) {
        return saves.emulatorName + " · Empty";
    }
    return saves.emulatorName + " · " + momentText(*modified) + " · " + ui::sizeText(saves.bytes());
}

std::size_t wrapped(std::size_t at, int step, std::size_t count) {
    const auto size = static_cast<long>(count);
    return static_cast<std::size_t>(((static_cast<long>(at) + step) % size + size) % size);
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

std::vector<EmulatorSaves> SaveCopyController::savesOf(const library::Game& game) const {
    return library::saves::savesOf(game, downloads_.setup().catalog, runnerSaves_);
}

std::string SaveCopyController::offer(const library::Game& game) const {
    const std::vector<EmulatorSaves> found = savesOf(game);
    if (found.size() < 2) {
        return {};
    }
    std::string names;
    for (const EmulatorSaves& saves : found) {
        names += (names.empty() ? "" : ", ") + saves.emulatorName;
    }
    return names;
}

void SaveCopyController::open(const library::Game& game) {
    std::vector<EmulatorSaves> found = savesOf(game);
    std::optional<library::saves::SaveKey> key = library::saves::keyOf(game);
    if (isOpen() || found.size() < 2 || !key) {
        return;
    }
    opened_ = Opened{.game = game, .key = std::move(*key)};
    saves_ = std::move(found);
    // The newest saves are the ones the player most likely wants to carry over.
    from_ = 0;
    for (std::size_t i = 1; i < saves_.size(); ++i) {
        if (saves_[i].modified() > saves_[from_].modified()) {
            from_ = i;
        }
    }
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

std::vector<std::size_t> SaveCopyController::targets() const {
    std::vector<std::size_t> places;
    for (std::size_t i = 0; i < saves_.size(); ++i) {
        if (i != from_) {
            places.push_back(i);
        }
    }
    return places;
}

std::vector<ui::SettingsRow> SaveCopyController::rows() const {
    std::vector<ui::SettingsRow> rows;
    if (saves_.size() < 2 || !opened_) {
        return rows;
    }
    const std::string& title = opened_->game.title;
    const EmulatorSaves& from = saves_[from_];
    const EmulatorSaves& to = saves_[to_];
    ui::Options sources;
    for (const EmulatorSaves& saves : saves_) {
        sources.labels.push_back(savesText(saves));
    }
    sources.chosen = from_;
    rows.push_back(ui::makeChoice(std::string{fromId}, "Copy from", "The saves to keep", sources));
    ui::Options places;
    const std::vector<std::size_t> others = targets();
    for (const std::size_t place : others) {
        places.labels.push_back(savesText(saves_[place]));
    }
    places.chosen = static_cast<std::size_t>(std::ranges::find(others, to_) - others.begin());
    rows.push_back(ui::makeChoice(std::string{toId}, "Copy to", "The saves replaced", places));
    ui::SettingsRow copy = ui::makeRow(std::string{copyId}, RowKind::Action, "Copy saves",
                                       to.emulatorName + "'s saves are backed up first", {});
    copy.confirm = from.emulatorName + "'s saves of " + title + " replace " + to.emulatorName +
                   "'s. " + to.emulatorName + "'s are backed up first and can be put back here.";
    copy.confirmVerb = "Copy";
    rows.push_back(std::move(copy));
    if (const std::optional<library::saves::SaveBackup> backup =
            copier_.latest(opened_->key, to.emulatorId)) {
        ui::SettingsRow undo = ui::makeRow(std::string{undoId}, RowKind::Action, "Undo copy",
                                           "Puts back " + to.emulatorName +
                                               "'s saves from before " + momentText(backup->made),
                                           {});
        undo.confirm = to.emulatorName + "'s saves of " + title +
                       " go back to how they were before " + backup->from + "'s were copied over.";
        undo.confirmVerb = "Undo";
        rows.push_back(std::move(undo));
    }
    rows.push_back(ui::makeRow(std::string{closeId}, RowKind::Action, "Close", {}, {}));
    return rows;
}

void SaveCopyController::run(const std::string& id, int step) {
    if (saves_.size() < 2 || !opened_) {
        return;
    }
    if (id == fromId) {
        const std::string target = saves_[to_].emulatorId;
        from_ = wrapped(from_, step, saves_.size());
        const std::vector<std::size_t> others = targets();
        const auto same = std::ranges::find_if(others, [this, &target](std::size_t place) {
            return saves_[place].emulatorId == target;
        });
        to_ = same == others.end() ? others.front() : *same;
    } else if (id == toId) {
        const std::vector<std::size_t> others = targets();
        const auto at = static_cast<std::size_t>(std::ranges::find(others, to_) - others.begin());
        to_ = others[wrapped(at, step, others.size())];
    } else if (id == copyId) {
        copy(*opened_);
    } else if (id == undoId) {
        undo(*opened_);
    } else if (id == closeId) {
        close();
    }
}

void SaveCopyController::copy(const Opened& opened) {
    const EmulatorSaves& from = saves_[from_];
    const EmulatorSaves& to = saves_[to_];
    const std::string failed = copier_.copy(opened.key, from, to, std::chrono::system_clock::now());
    hooks_.say(failed.empty() ? from.emulatorName + "'s saves copied to " + to.emulatorName
                              : failed,
               !failed.empty());
    reread(opened.game);
}

void SaveCopyController::undo(const Opened& opened) {
    const EmulatorSaves& to = saves_[to_];
    const std::string failed = copier_.undo(opened.key, to.emulatorId);
    hooks_.say(failed.empty() ? to.emulatorName + "'s saves are back as they were" : failed,
               !failed.empty());
    reread(opened.game);
}

void SaveCopyController::reread(const library::Game& game) {
    const std::string source = saves_[from_].emulatorId;
    const std::string target = saves_[to_].emulatorId;
    saves_ = savesOf(game);
    hooks_.changed();
    if (saves_.size() < 2) {
        close();
        return;
    }
    const auto place = [this](const std::string& id, std::size_t fallback) {
        const auto found = std::ranges::find(saves_, id, &EmulatorSaves::emulatorId);
        return found == saves_.end() ? fallback : static_cast<std::size_t>(found - saves_.begin());
    };
    from_ = place(source, 0);
    to_ = place(target, targets().front());
    if (to_ == from_) {
        to_ = targets().front();
    }
}

} // namespace opensu::app
