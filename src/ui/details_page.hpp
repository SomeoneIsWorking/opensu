// details_page — the page a game opens on (A on its tile, Y, or Details in its menu): the cover and
// title, what is known about the game, and the buttons that act on it. opensu's own; iiSU shows a
// game's details only on the second screen of a dual-display device. Pure state and geometry,
// tested without a window; `DetailsPagePainter` draws it.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "home_layout.hpp"
#include "library/game.hpp"

namespace opensu::ui {

enum class DetailsAction : std::uint8_t {
    /// Play an installed game, or offer to install one that is not.
    Launch,
    /// Update an installed store game its store has a newer build of.
    Update,
    /// Choose the next emulator for a ROM.
    Emulator,
    /// Download the emulator a ROM is missing, or cancel its download.
    DownloadEmulator,
    /// Give a Windows GOG or Epic install its own Wine prefix, or back to the shared one.
    WinePrefix,
    /// Hide the game, or show it again.
    Hidden,
    /// Uninstall an installed store game, on a second press.
    Uninstall,
    /// Open the game's settings over the page.
    GameSettings,
    /// Pick the game's cover from SteamGridDB's.
    ChooseArtwork,
    /// Edit the game's button mapping.
    Controls,
    Back,
};

/// A button on the page. `value` is the choice a button shows beside its label, empty for none.
struct DetailsButton {
    DetailsAction action{DetailsAction::Back};
    std::string label;
    std::string value;

    bool operator==(const DetailsButton&) const = default;
};

/// One line of what is known about the game.
struct DetailsRow {
    std::string label;
    std::string value;

    bool operator==(const DetailsRow&) const = default;
};

/// Everything the page shows for one game.
struct DetailsView {
    std::string gameId;
    std::string title;
    /// The store or system the game is from, as the badge reads it.
    std::string badge;
    std::vector<DetailsRow> rows;
    std::vector<DetailsButton> buttons;

    bool operator==(const DetailsView&) const = default;
};

/// When the game was last played, in words: "Never played", "3 days ago", or the date past a month.
[[nodiscard]] std::string
lastPlayedText(const std::optional<std::chrono::system_clock::time_point>& at,
               std::chrono::system_clock::time_point now);

/// The emulator a ROM that cannot start can download: its pick when that is missing and openSU
/// can download it, else the first such emulator of its system; null for none.
[[nodiscard]] const library::EmulatorOption* emulatorToDownload(const library::Game& game);

/// The game's settings as the page shows them.
struct GameSettingsView {
    /// Where the game's emulator comes from ("Automatic (romset)", "From Super Nintendo"), added
    /// to the Emulator row; empty for none.
    std::string emulatorSource;
    /// After the Emulator row: the value of each setting a layer keeps and where it comes from.
    std::vector<DetailsRow> rows;
    /// After the Emulator button: `GameSettings` when the game has anything to set.
    std::vector<DetailsButton> buttons;
};

/// What the page shows for `game`; `hidden` is whether the player hid it. `download` is what the
/// Download button reads beside its label: the running download's progress, or empty.
/// `ownPrefix` is whether a Windows install runs in a Wine prefix of its own.
[[nodiscard]] DetailsView detailsFor(const library::Game& game, bool hidden,
                                     std::chrono::system_clock::time_point now,
                                     const std::string& download = {}, bool ownPrefix = false,
                                     const GameSettingsView& settings = {});

/// The page and its buttons in pixels.
struct DetailsLayout {
    /// The cover's box.
    Rect art;
    /// Where the title, badge and rows flow, right of the cover and above the buttons.
    Rect text;
    /// The column right of the buttons, from the top to the cover's bottom: where the rows go,
    /// below the title and badge, when they do not fit above the buttons.
    Rect side;
    std::vector<Rect> buttons;
    float radius{};

    /// The button under the point, or nothing.
    [[nodiscard]] std::optional<std::size_t> buttonAt(float x, float y) const noexcept;
};

/// What the page lays out besides its frame.
struct DetailsParts {
    /// The pixels the title and badge take above the buttons.
    float header{};
    std::size_t buttons{};
};

/// The page between `topInset` and `bottomInset` in a `width` x `height` frame at `dp` pixels per
/// dp. The buttons stay below the header, growing shorter when they would not fit beneath it.
[[nodiscard]] DetailsLayout layoutDetails(float width, float height, float dp, float topInset,
                                          float bottomInset, DetailsParts parts);

class DetailsPage {
  public:
    /// Opens on `view`, focused on the first button.
    void open(DetailsView view);
    /// Shows `view` in place of the one open, keeping focus on the same action when it is still
    /// there, armed while that is still focused. The page closes itself when `view` is for another
    /// game.
    void refresh(DetailsView view);
    void close() noexcept {
        open_ = false;
        disarm();
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return open_;
    }

    [[nodiscard]] const DetailsView& view() const noexcept {
        return view_;
    }
    [[nodiscard]] std::size_t focus() const noexcept {
        return focus_;
    }
    /// Moves focus by `delta` buttons, stopping at the ends; reports whether it moved. Moving lets
    /// go of an armed button.
    bool move(int delta) noexcept;
    /// Focuses button `index`; reports whether focus changed, which lets go of an armed button.
    bool focusButton(std::size_t index) noexcept;
    [[nodiscard]] DetailsAction selected() const noexcept {
        return view_.buttons[focus_].action;
    }

    /// Asks the focused button for a second press; `confirm` says what that press does.
    void arm(std::string confirm);
    void disarm() noexcept {
        confirm_.clear();
    }
    [[nodiscard]] bool armed() const noexcept {
        return !confirm_.empty();
    }
    /// What the armed button's second press does; empty when none is armed.
    [[nodiscard]] const std::string& confirm() const noexcept {
        return confirm_;
    }
    /// Button `index`'s label as the page reads it: an armed one asks for the second press.
    [[nodiscard]] std::string shown(std::size_t index) const;
    /// The rows as the page reads them: an armed button puts what its second press does first.
    [[nodiscard]] std::vector<DetailsRow> rows() const;

  private:
    DetailsView view_;
    std::size_t focus_{0};
    /// What the armed button's second press does; empty when none is armed.
    std::string confirm_;
    bool open_{false};
};

} // namespace opensu::ui
