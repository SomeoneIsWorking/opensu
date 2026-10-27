#include "folder_chooser.hpp"

#include <algorithm>
#include <cctype>

namespace opensu::ui {
namespace {

namespace fs = std::filesystem;

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace

void FolderChooser::open(const fs::path& start, Accept files) {
    accept_ = std::move(files);
    std::error_code error;
    current_ = start.lexically_normal();
    while (!current_.empty() && current_ != current_.root_path() &&
           !fs::is_directory(current_, error)) {
        current_ = current_.parent_path();
    }
    if (current_.empty() || !fs::is_directory(current_, error)) {
        current_ = "/";
    }
    list();
}

void FolderChooser::list() {
    folders_.clear();
    files_.clear();
    problem_.clear();
    std::error_code error;
    fs::directory_iterator item{current_, error};
    for (; !error && item != fs::directory_iterator{}; item.increment(error)) {
        std::error_code status;
        const std::string name = item->path().filename().string();
        if (name.starts_with('.')) {
            continue;
        }
        if (item->is_directory(status)) {
            folders_.push_back(FolderEntry{name, item->path()});
        } else if (accept_ && accept_(item->path())) {
            files_.push_back(FolderEntry{name, item->path()});
        }
    }
    if (error) {
        problem_ = "cannot list this folder: " + error.message();
    }
    const auto byName = [](const FolderEntry& a, const FolderEntry& b) {
        return lowered(a.name) < lowered(b.name);
    };
    std::ranges::sort(folders_, byName);
    std::ranges::sort(files_, byName);
}

bool FolderChooser::up() {
    if (!hasParent()) {
        return false;
    }
    current_ = current_.parent_path();
    list();
    return true;
}

void FolderChooser::enter(const fs::path& folder) {
    current_ = folder;
    list();
}

} // namespace opensu::ui
