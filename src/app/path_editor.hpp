// path_editor — choosing a folder on the folder chooser card: the path field at its top (typed on
// the on-screen keyboard or a physical one, Submit takes it), Use this folder, the entry that drops
// the setting when the request has one, Up one level and the folders to go into. One request at a
// time: what it is for, where it starts, what makes a folder acceptable and what to do with the
// answer. A folder that is refused stays on screen with the reason in a toast.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
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
        /// Why a folder is refused, or empty when it is fine.
        std::function<std::string(const std::filesystem::path&)> refusal;
        /// Takes the choice: the folder, or nothing when the player dropped it.
        std::function<void(std::optional<std::filesystem::path>)> done;
    };

    /// What the editor tells the player: `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    PathEditor(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome,
               audio::SdlSoundPlayer& sounds, Say say)
        : navigator_{navigator}, chrome_{chrome}, sounds_{sounds}, say_{std::move(say)} {
    }

    /// Opens the chooser for `request`.
    void begin(Request request);
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
    }
    /// Takes the typed path.
    void submitTyped();
    /// Takes the folder shown.
    void useShown();
    /// Drops the setting being chosen.
    void dropChoice();
    /// Shows `folder`, one of the folders listed.
    void enter(const std::filesystem::path& folder);
    /// Goes up a level; reports whether there was one.
    bool up();
    /// Closes the chooser without an answer.
    void cancel();

  private:
    /// Takes `folder` if the request accepts it, else says why not.
    void choose(const std::filesystem::path& folder);
    /// Closes the card with iiSU's Close sound.
    void close();
    /// Another folder shows: the field follows it and the focus goes to Use this folder.
    void shown();

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    audio::SdlSoundPlayer& sounds_;
    Say say_;
    Request request_;
    ui::FolderChooser folders_;
    std::string typed_;
};

} // namespace opensu::app
