// folder_chooser — a folder picker for a pad: the folders under the current one as a list, with
// entries to use the folder shown, to go up a level and to type a path instead. opensu's own; iiSU
// picks its games folder with Android's document picker. Pure state and geometry over the
// filesystem, tested without a window; `FolderChooserPainter` draws it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"

namespace opensu::ui {

enum class FolderEntryKind : std::uint8_t {
    /// Choose the folder being shown.
    Use,
    /// Type a path on the on-screen keyboard instead.
    Type,
    /// Drop the setting being chosen, so it follows what it falls back to.
    Clear,
    /// Go up a level.
    Parent,
    /// Go into a folder.
    Folder,
};

struct FolderEntry {
    FolderEntryKind kind{FolderEntryKind::Folder};
    std::string label;
    std::filesystem::path path;

    bool operator==(const FolderEntry&) const = default;
};

/// What pressing an entry asked for.
struct FolderPress {
    /// The folder the player chose.
    std::optional<std::filesystem::path> chosen;
    /// The player wants to type a path.
    bool type{false};
    /// The player wants the setting dropped.
    bool clear{false};
};

/// Where the chooser's parts stand in a frame, in pixels.
struct FolderChooserLayout {
    Rect panel;
    Rect title;
    Rect path;
    /// The part of the panel the entries scroll in.
    Rect list;
    /// The entries, scrolled; one may stand partly outside `list`.
    std::vector<Rect> rows;
    Rect hints;
    float radius{};
    float rowRadius{};

    [[nodiscard]] std::optional<std::size_t> rowAt(float x, float y) const noexcept;
};

/// How many entries the list holds and which one is focused.
struct FolderWalk {
    std::size_t entries{};
    std::size_t focused{};
};

[[nodiscard]] FolderChooserLayout layoutFolderChooser(const Rect& frame, float dp,
                                                      const FolderWalk& walk);

class FolderChooser {
  public:
    /// Opens on `start`, or its nearest folder above that exists, called `title`. The first entry
    /// is focused. A non-empty `clearLabel` adds an entry of that name that drops the setting.
    void open(std::string title, const std::filesystem::path& start, std::string clearLabel = {});
    void close() noexcept {
        open_ = false;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    /// The folder being shown.
    [[nodiscard]] const std::filesystem::path& current() const noexcept {
        return current_;
    }
    /// Why the folder could not be listed; empty when it could.
    [[nodiscard]] const std::string& problem() const noexcept {
        return problem_;
    }
    [[nodiscard]] const std::vector<FolderEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }

    /// Moves focus `delta` entries, stopping at the ends; reports whether it moved.
    bool move(int delta) noexcept;
    bool focusEntry(std::size_t index) noexcept;
    /// Goes up a level; reports whether there was one.
    bool up();
    /// Presses the focused entry: goes into or up out of a folder, which asks for nothing, or
    /// chooses the folder, asks to type or asks to drop the setting.
    FolderPress press();

    [[nodiscard]] FolderChooserLayout layout(const Rect& frame, float dp) const {
        return layoutFolderChooser(frame, dp, FolderWalk{entries_.size(), focus_});
    }

  private:
    /// Lists the folders under `current_`.
    void list();

    std::string title_;
    std::string clearLabel_;
    std::filesystem::path current_;
    std::string problem_;
    std::vector<FolderEntry> entries_;
    std::size_t focus_{0};
    bool open_{false};
};

} // namespace opensu::ui
