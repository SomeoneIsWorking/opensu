#include "folder_chooser.hpp"

#include <algorithm>
#include <cctype>

namespace opensu::ui {
namespace {

namespace fs = std::filesystem;

constexpr float panelWidthDp = 523.6f;
constexpr float panelPaddingDp = 16.0f;
constexpr float titleHeightDp = 30.0f;
constexpr float pathHeightDp = 26.0f;
constexpr float rowHeightDp = 40.0f;
constexpr float rowGapDp = 4.0f;
constexpr float hintHeightDp = 28.0f;
constexpr float sectionGapDp = 8.0f;
constexpr float radiusDp = 17.0f;
constexpr float rowRadiusDp = 10.0f;
constexpr float panelMostHeight = 0.92f;

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace

FolderChooserLayout layoutFolderChooser(const Rect& frame, float dp, const FolderWalk& walk) {
    const std::size_t entries = walk.entries;
    const std::size_t focused = walk.focused;
    FolderChooserLayout layout;
    const float pad = panelPaddingDp * dp;
    const float gap = sectionGapDp * dp;
    const float rowStep = (rowHeightDp + rowGapDp) * dp;
    const float chrome =
        2.0f * pad + (titleHeightDp + pathHeightDp + hintHeightDp) * dp + 3.0f * gap;
    const float width = std::min(panelWidthDp * dp, frame.width);
    const float most = frame.height * panelMostHeight;
    const auto fitting = static_cast<std::size_t>(std::max((most - chrome) / rowStep, 3.0f));
    const std::size_t shown = std::min(std::max(entries, std::size_t{3}), fitting);
    const float listHeight = static_cast<float>(shown) * rowStep - rowGapDp * dp;
    const float height = chrome + listHeight;
    layout.panel = Rect{frame.x + (frame.width - width) * 0.5f,
                        frame.y + (frame.height - height) * 0.5f, width, height};
    layout.radius = radiusDp * dp;
    layout.rowRadius = rowRadiusDp * dp;
    float y = layout.panel.y + pad;
    layout.title = Rect{layout.panel.x + pad, y, width - 2.0f * pad, titleHeightDp * dp};
    y += titleHeightDp * dp;
    layout.path = Rect{layout.panel.x + pad, y, width - 2.0f * pad, pathHeightDp * dp};
    y += pathHeightDp * dp + gap;
    layout.list = Rect{layout.panel.x + pad, y, width - 2.0f * pad, listHeight};
    const std::size_t first = focused >= shown ? focused + 1 - shown : 0;
    for (std::size_t i = 0; i < entries; ++i) {
        layout.rows.push_back(
            Rect{layout.list.x,
                 layout.list.y + (static_cast<float>(i) - static_cast<float>(first)) * rowStep,
                 layout.list.width, rowHeightDp * dp});
    }
    y += listHeight + gap;
    layout.hints = Rect{layout.panel.x + pad, y, width - 2.0f * pad, hintHeightDp * dp};
    return layout;
}

std::optional<std::size_t> FolderChooserLayout::rowAt(float x, float y) const noexcept {
    if (!list.contains(x, y)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

void FolderChooser::open(std::string title, const fs::path& start, std::string clearLabel) {
    title_ = std::move(title);
    clearLabel_ = std::move(clearLabel);
    std::error_code error;
    current_ = start.lexically_normal();
    while (!current_.empty() && current_ != current_.root_path() &&
           !fs::is_directory(current_, error)) {
        current_ = current_.parent_path();
    }
    if (current_.empty() || !fs::is_directory(current_, error)) {
        current_ = "/";
    }
    open_ = true;
    list();
}

void FolderChooser::list() {
    entries_.clear();
    problem_.clear();
    entries_.push_back(FolderEntry{FolderEntryKind::Use, "Use this folder", current_});
    entries_.push_back(FolderEntry{FolderEntryKind::Type, "Type a path...", {}});
    if (!clearLabel_.empty()) {
        entries_.push_back(FolderEntry{FolderEntryKind::Clear, clearLabel_, {}});
    }
    if (current_.has_relative_path()) {
        entries_.push_back(
            FolderEntry{FolderEntryKind::Parent, "Up one level", current_.parent_path()});
    }
    std::error_code error;
    std::vector<FolderEntry> folders;
    fs::directory_iterator item{current_, error};
    for (; !error && item != fs::directory_iterator{}; item.increment(error)) {
        std::error_code status;
        const std::string name = item->path().filename().string();
        if (item->is_directory(status) && !name.starts_with('.')) {
            folders.push_back(FolderEntry{FolderEntryKind::Folder, name, item->path()});
        }
    }
    if (error) {
        problem_ = "cannot list this folder: " + error.message();
    }
    std::ranges::sort(folders, [](const FolderEntry& a, const FolderEntry& b) {
        return lowered(a.label) < lowered(b.label);
    });
    entries_.insert(entries_.end(), folders.begin(), folders.end());
    focus_ = 0;
}

bool FolderChooser::move(int delta) noexcept {
    const int last = static_cast<int>(entries_.size()) - 1;
    return focusEntry(
        static_cast<std::size_t>(std::clamp(static_cast<int>(focus_) + delta, 0, last)));
}

bool FolderChooser::focusEntry(std::size_t index) noexcept {
    if (index >= entries_.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    return true;
}

bool FolderChooser::up() {
    if (!current_.has_relative_path()) {
        return false;
    }
    current_ = current_.parent_path();
    list();
    return true;
}

FolderPress FolderChooser::press() {
    const FolderEntry entry = entries_[focus_];
    FolderPress result;
    switch (entry.kind) {
    case FolderEntryKind::Use:
        result.chosen = current_;
        return result;
    case FolderEntryKind::Type:
        result.type = true;
        return result;
    case FolderEntryKind::Clear:
        result.clear = true;
        return result;
    case FolderEntryKind::Parent:
    case FolderEntryKind::Folder:
        current_ = entry.path;
        list();
        break;
    }
    return result;
}

} // namespace opensu::ui
