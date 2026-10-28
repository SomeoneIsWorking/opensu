#include "path_editor.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "folder_chooser_screen.hpp"

namespace opensu::app {

bool PathEditor::active() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const FolderChooserScreen*>(screen) != nullptr;
    });
}

void PathEditor::begin(Request request) {
    request_ = std::move(request);
    if (system_ && !request_.files && system_->open()) {
        asking_ = true;
        return;
    }
    folders_.open(request_.start, request_.files);
    typed_ = folders_.current().string();
    refusal_.clear();
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<FolderChooserScreen>(*this, chrome_));
}

void PathEditor::poll() {
    if (!asking_) {
        return;
    }
    const std::optional<std::string> answer = system_->take();
    if (!answer) {
        return;
    }
    asking_ = false;
    // The OS picker hands back only folders the app was granted, so its answer needs no check.
    if (!answer->empty()) {
        request_.done(std::filesystem::path{*answer});
    }
}

void PathEditor::close() {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (dynamic_cast<const FolderChooserScreen*>(screen) != nullptr) {
            // input-sound.md 3.4 Close: the player dismisses a panel.
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

void PathEditor::cancel() {
    close();
}

void PathEditor::choose(const std::filesystem::path& folder) {
    refusal_ = request_.refusal(folder);
    if (!refusal_.empty()) {
        return;
    }
    close();
    request_.done(folder);
}

void PathEditor::submitTyped() {
    choose(std::filesystem::path{typed_});
}

void PathEditor::useShown() {
    choose(folders_.current());
}

void PathEditor::pick(const std::filesystem::path& file) {
    choose(file);
}

void PathEditor::dropChoice() {
    close();
    request_.done(std::nullopt);
}

void PathEditor::enter(const std::filesystem::path& folder) {
    folders_.enter(folder);
    shown();
}

bool PathEditor::up() {
    if (!folders_.up()) {
        return false;
    }
    shown();
    return true;
}

void PathEditor::shown() {
    typed_ = folders_.current().string();
    refusal_.clear();
    // input-sound.md 3.4 Navigation: a focus move in a list.
    sounds_.play(audio::Effect::Navigation);
    navigator_.focus(FolderChooserScreen::landing(*this));
}

} // namespace opensu::app
