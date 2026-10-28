// path_editor — choosing a folder, or a file, on the folder chooser card: the path field at its top
// (typed on the on-screen keyboard or a physical one, Submit takes it), Use this folder, the entry
// that drops the setting when the request has one, Up one level, the folders to go into and, for a
// file, the files to pick. One request at a time: what it is for, where it starts, what makes an
// answer acceptable and what to do with it. An answer that is refused stays on screen with the
// reason on the card, under the field.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "system_folder_picker.hpp"
#include "ui/folder_chooser.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class PathEditor {
  public:
    /// What one choice asks.
    struct Request {
        /// The chooser's title ("Default install folder").
        std::string title;
        /// Where the chooser opens: the folder now chosen, or where to look first.
        std::filesystem::path start;
        /// The entry that drops the choice ("Follow the default"); empty for none.
        std::string clearLabel;
        /// Why a folder (or file) is refused, or empty when it is fine.
        std::function<std::string(const std::filesystem::path&)> refusal;
        /// When set, a file is chosen, not a folder: the files it accepts are listed to pick.
        ui::FolderChooser::Accept files;
        /// Takes the choice: the folder, or nothing when the player dropped it.
        std::function<void(std::optional<std::filesystem::path>)> done;
    };

    PathEditor(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome,
               audio::SdlSoundPlayer& sounds)
        : navigator_{navigator}, chrome_{chrome}, sounds_{sounds} {
    }

    /// Opens the chooser for `request`; a folder is asked of the OS's own picker where it has one.
    void begin(Request request);
    /// Hands the OS picker's answer to the request once the picker closed.
    void poll();
    /// Whether the chooser is up.
    [[nodiscard]] bool active() const;

    // What the card asks.
    [[nodiscard]] const Request& request() const noexcept {
        return request_;
    }
    [[nodiscard]] const ui::FolderChooser& folders() const noexcept {
        return folders_;
    }
    /// The path in the field: the folder shown, or what was typed over it.
    [[nodiscard]] const std::string& typed() const noexcept {
        return typed_;
    }
    void type(std::string text) {
        typed_ = std::move(text);
        refusal_.clear();
    }
    /// Why the last path offered was not taken, shown on the card; empty when none was refused.
    [[nodiscard]] const std::string& refusal() const noexcept {
        return refusal_;
    }
    /// Takes the typed path.
    void submitTyped();
    /// Takes the folder shown.
    void useShown();
    /// Takes `file`, one of the files listed.
    void pick(const std::filesystem::path& file);
    /// Drops the setting being chosen.
    void dropChoice();
    /// Shows `folder`, one of the folders listed.
    void enter(const std::filesystem::path& folder);
    /// Goes up a level; reports whether there was one.
    bool up();
    /// Closes the chooser without an answer.
    void cancel();

  private:
    /// Takes `folder` if the request accepts it, else keeps why not for the card.
    void choose(const std::filesystem::path& folder);
    /// Closes the card with iiSU's Close sound.
    void close();
    /// Another folder shows: the field follows it and the focus goes to Use this folder.
    void shown();

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    audio::SdlSoundPlayer& sounds_;
    Request request_;
    ui::FolderChooser folders_;
    std::string typed_;
    std::string refusal_;
    std::unique_ptr<SystemFolderPicker> system_{openSystemFolderPicker()};
    /// The OS picker is up for `request_`.
    bool asking_{false};
};

} // namespace opensu::app
