// folder_chooser — the folders a pad picks a folder among: the one being shown and the folders
// under it, going in and up. opensu's own; iiSU picks its games folder with Android's document
// picker. Pure state over the filesystem, tested without a window; `app/folder_chooser_screen`
// shows it.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace opensu::ui {

/// A folder under the one shown.
struct FolderEntry {
    std::string name;
    std::filesystem::path path;

    bool operator==(const FolderEntry&) const = default;
};

class FolderChooser {
  public:
    /// Shows `start`, or its nearest folder above that exists.
    void open(const std::filesystem::path& start);

    /// The folder being shown.
    [[nodiscard]] const std::filesystem::path& current() const noexcept {
        return current_;
    }
    /// Why the folder could not be listed; empty when it could.
    [[nodiscard]] const std::string& problem() const noexcept {
        return problem_;
    }
    /// The folders under it, without hidden ones, sorted without case.
    [[nodiscard]] const std::vector<FolderEntry>& folders() const noexcept {
        return folders_;
    }
    /// Whether there is a level above it.
    [[nodiscard]] bool hasParent() const {
        return current_.has_relative_path();
    }
    /// Goes up a level; reports whether there was one.
    bool up();
    /// Shows `folder`, one of `folders()`.
    void enter(const std::filesystem::path& folder);

  private:
    /// Lists the folders under `current_`.
    void list();

    std::filesystem::path current_;
    std::string problem_;
    std::vector<FolderEntry> folders_;
};

} // namespace opensu::ui
