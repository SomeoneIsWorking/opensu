// path_editor — choosing a folder on the folder chooser, with the on-screen keyboard (or a physical
// keyboard) for typing a path instead. One request at a time: what it is for, where it starts, what
// makes a folder acceptable and what to do with the answer. A folder that is refused stays on
// screen with the reason in a toast.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "audio/sound_player.hpp"
#include "gamepad/event.hpp"
#include "ui/folder_chooser.hpp"
#include "ui/search_panel.hpp"

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

    PathEditor(ui::FolderChooser& folders, ui::SearchPanel& entry, audio::SoundPlayer& sounds,
               Say say)
        : folders_{folders}, entry_{entry}, sounds_{sounds}, say_{std::move(say)} {
    }

    /// Opens the chooser for `request`.
    void begin(Request request);
    /// Whether a chooser or the keyboard is up, which is when it takes every button.
    [[nodiscard]] bool active() const noexcept {
        return folders_.isOpen() || entry_.isOpen();
    }
    /// Whether the keyboard is up, which a physical keyboard types into.
    [[nodiscard]] bool typing() const noexcept {
        return entry_.isOpen();
    }

    /// A pad button while it is up.
    void act(gamepad::Button button);

    /// What a physical keyboard typed while the keyboard is up.
    void typeText(std::string_view text);
    void backspace();
    /// Enter: takes the typed path.
    void confirm();
    /// Escape: closes the keyboard, back to the chooser.
    void dismiss();

  private:
    void actInChooser(gamepad::Button button);
    void actOnKeys(gamepad::Button button);
    /// Closes the chooser and the keyboard without an answer.
    void cancel();
    /// Takes `folder` if the request accepts it, else says why not.
    void choose(const std::filesystem::path& folder);
    void openEntry();
    void closeEntry();

    ui::FolderChooser& folders_;
    ui::SearchPanel& entry_;
    audio::SoundPlayer& sounds_;
    Say say_;
    Request request_;
};

} // namespace opensu::app
