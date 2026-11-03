#include "import_picks.hpp"

#include <algorithm>
#include <utility>

namespace opensu::app {

ImportPicks::ImportPicks(std::vector<library::own::Entry> found)
    : found_{std::move(found)}, chosen_(found_.size(), true) {
}

std::size_t ImportPicks::chosenCount() const noexcept {
    return static_cast<std::size_t>(std::ranges::count(chosen_, true));
}

void ImportPicks::change(std::vector<bool> next) {
    if (next == chosen_) {
        return;
    }
    history_.push_back(std::exchange(chosen_, std::move(next)));
}

void ImportPicks::choose(std::size_t index, bool chosen) {
    std::vector<bool> next = chosen_;
    next.at(index) = chosen;
    change(std::move(next));
}

void ImportPicks::chooseAll() {
    change(std::vector<bool>(found_.size(), true));
}

void ImportPicks::chooseNone() {
    change(std::vector<bool>(found_.size(), false));
}

bool ImportPicks::undo() {
    if (history_.empty()) {
        return false;
    }
    chosen_ = std::move(history_.back());
    history_.pop_back();
    return true;
}

std::vector<library::own::Entry> ImportPicks::picked() const {
    std::vector<library::own::Entry> picked;
    for (std::size_t i = 0; i < found_.size(); ++i) {
        if (chosen_[i]) {
            picked.push_back(found_[i]);
        }
    }
    return picked;
}

} // namespace opensu::app
