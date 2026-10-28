#include "mapping_page.hpp"

#include <algorithm>
#include <optional>

#include "gpu2d/ui/focus.hpp"
#include "paint.hpp"

namespace opensu::ui {

void MappingPage::open(std::string title, std::vector<MappingItem> items, std::string note) {
    title_ = std::move(title);
    items_ = std::move(items);
    note_ = std::move(note);
    const auto button = std::ranges::find_if(items_, [](const MappingItem& item) {
        return item.button != gamepad::Button::None;
    });
    focus_ = button == items_.end() ? 0 : static_cast<std::size_t>(button - items_.begin());
    mode_ = MappingMode::Board;
    pick_ = {};
    prompt_.clear();
    open_ = true;
}

void MappingPage::refresh(std::vector<MappingItem> items, std::string note) {
    const std::string id = focused() != nullptr ? focused()->id : std::string{};
    items_ = std::move(items);
    note_ = std::move(note);
    const auto same = std::ranges::find(items_, id, &MappingItem::id);
    if (same != items_.end()) {
        focus_ = static_cast<std::size_t>(same - items_.begin());
    } else if (!items_.empty()) {
        focus_ = std::min(focus_, items_.size() - 1);
    } else {
        focus_ = 0;
    }
}

std::string_view MappingPage::state() const noexcept {
    if (!open_) {
        return {};
    }
    switch (mode_) {
    case MappingMode::Board:
        return "board";
    case MappingMode::Picking:
        return "picking";
    case MappingMode::Listening:
        return "listening";
    }
    return {};
}

std::string_view MappingPage::focusName() const noexcept {
    if (!open_) {
        return {};
    }
    if (mode_ == MappingMode::Picking) {
        return choice_ < pick_.choices.size() ? std::string_view{pick_.choices[choice_].label}
                                              : std::string_view{};
    }
    return focus_ < items_.size() ? std::string_view{items_[focus_].id} : std::string_view{};
}

const MappingItem* MappingPage::focused() const {
    return focus_ < items_.size() ? &items_[focus_] : nullptr;
}

bool MappingPage::move(Direction direction) {
    if (mode_ == MappingMode::Listening) {
        return false;
    }
    const bool picking = mode_ == MappingMode::Picking;
    const std::vector<Rect> laid =
        picking ? pickerFocusBoxes(pick_.choices) : mappingFocusBoxes(items_);
    std::size_t& at = picking ? choice_ : focus_;
    if (at >= laid.size()) {
        return false;
    }
    std::vector<gpu2d::Rect> boxes;
    boxes.reserve(laid.size());
    for (const Rect& box : laid) {
        boxes.push_back(onCanvas(box));
    }
    const std::optional<std::size_t> next =
        gpu2d::ui::nearestInDirection(boxes, boxes[at], direction);
    if (!next) {
        return false;
    }
    at = *next;
    return true;
}

void MappingPage::pick(MappingPick pick) {
    pick_ = std::move(pick);
    choice_ = pick_.choices.empty() ? 0 : std::min(pick_.current, pick_.choices.size() - 1);
    mode_ = MappingMode::Picking;
}

void MappingPage::listen(std::string prompt) {
    prompt_ = std::move(prompt);
    mode_ = MappingMode::Listening;
}

void MappingPage::showBoard(std::string note) {
    note_ = std::move(note);
    mode_ = MappingMode::Board;
    pick_ = {};
    prompt_.clear();
}

} // namespace opensu::ui
