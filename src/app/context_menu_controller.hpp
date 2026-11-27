// context_menu_controller — the menu a focused tile opens (iiSU: SELECT on a ROM, `pb0.java:2086`;
// right click on a tile) and what its entries do. The tile it is open for is kept, so an entry
// acts on it whatever the grid does meanwhile. Each entry closes the menu and does its one thing;
// Uninstall and Remove ask first through `confirm`, naming what goes. A sub-library (a store,
// another openSU, the player's own games, a console) offers Configure, and one the shell says is
// removable (an integration that is on, another openSU) Remove.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "library/shelf.hpp"
#include "preferences.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_images.hpp"
#include "ui/setting_row.hpp"
#include "uninstall_prompt.hpp"
#include "update_request.hpp"

namespace opensu::app {

/// The ids of the menu's entries.
inline constexpr std::string_view menuLaunchId = "menu.launch";
inline constexpr std::string_view menuInstallId = "menu.install";
inline constexpr std::string_view menuUpdateId = "menu.update";
inline constexpr std::string_view menuDetailsId = "menu.details";
inline constexpr std::string_view menuHideId = "menu.hide";
inline constexpr std::string_view menuUnhideId = "menu.unhide";
inline constexpr std::string_view menuAddHomeId = "menu.addHome";
inline constexpr std::string_view menuRemoveHomeId = "menu.removeHome";
inline constexpr std::string_view menuUninstallId = "menu.uninstall";
inline constexpr std::string_view menuOpenId = "menu.open";
inline constexpr std::string_view menuRefreshId = "menu.refresh";
inline constexpr std::string_view menuConfigureId = "menu.configure";
inline constexpr std::string_view menuRemoveId = "menu.remove";

/// What the menu offers for `item`, first entry first. `hidden` is whether a game is hidden now;
/// `onHome` whether the tile is on Home, nothing for one that cannot be put there; `removable`
/// whether a store's or openSU's tile can be taken out of the library.
[[nodiscard]] std::vector<ui::SettingsRow> contextEntriesFor(const library::ShelfItem& item,
                                                             bool hidden,
                                                             std::optional<bool> onHome,
                                                             bool removable = false);

class ContextMenuController final : public ui::RowActions {
  public:
    /// What the entries do, in the shell app's terms.
    struct Hooks {
        /// The tile the grid has focused, or null when none.
        std::function<const library::ShelfItem*()> focused;
        /// Launches the focused game, or offers to install it.
        std::function<void()> launch;
        std::function<void()> details;
        /// Hides or shows `game`, and shows the shelf again.
        std::function<void(const library::Game& game, bool hidden)> setHidden;
        /// Whether the tile is on Home; nothing for one that cannot be put there.
        std::function<std::optional<bool>(const library::ShelfItem& item)> onHome;
        /// Puts the tile on Home or takes it off, and shows the shelf again.
        std::function<void(const library::ShelfItem& item, bool on)> setOnHome;
        std::function<void(const library::Folder& folder)> open;
        std::function<void()> refresh;
        /// Opens where `folder` is set up: its integration, LAN, the game folders, its console.
        std::function<void(const library::Folder& folder)> configure;
        /// Whether `launcher` can be taken out of the library: an integration that is on, or
        /// another openSU.
        std::function<bool(const library::Launcher& launcher)> removable;
        /// Takes `launcher` out of the library: its store's integration off, or another openSU
        /// forgotten.
        std::function<void(const library::Launcher& launcher)> remove;
    };

    /// What the entries act on.
    struct Services {
        const Preferences& preferences;
        UninstallPrompt& uninstall;
        UpdateRequest& update;
    };

    /// The menu goes on `navigator`'s stack, its icons from `images`.
    ContextMenuController(gpu2d::ui::Navigator& navigator, ui::ScreenImages& images,
                          audio::SdlSoundPlayer& sounds, Services services, Hooks hooks)
        : navigator_{navigator}, images_{images}, sounds_{sounds},
          preferences_{services.preferences}, uninstall_{services.uninstall},
          update_{services.update}, hooks_{std::move(hooks)} {
    }

    /// Opens the menu for the focused tile; nothing when no tile has focus.
    void open();
    /// Closes the menu with iiSU's Close sound.
    void close();
    [[nodiscard]] bool isOpen() const;

    // What the screen asks.
    /// The tile's name, which heads the menu.
    [[nodiscard]] const std::string& title() const noexcept {
        return title_;
    }
    [[nodiscard]] const std::vector<ui::SettingsRow>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] ui::ScreenImages& images() const noexcept {
        return images_;
    }
    /// Closes the menu and does what entry `id` says to the tile it was opened for.
    void run(const std::string& id, int step) override;

  private:
    /// The menu on the stack, or null.
    [[nodiscard]] const gpu2d::ui::Screen* menu() const;
    /// Asks before uninstalling `game`, naming what goes, or says why it cannot be.
    void uninstall(const library::Game& game);
    /// Asks before taking `launcher` out of the library.
    void remove(const library::Launcher& launcher);

    gpu2d::ui::Navigator& navigator_;
    ui::ScreenImages& images_;
    audio::SdlSoundPlayer& sounds_;
    const Preferences& preferences_;
    UninstallPrompt& uninstall_;
    UpdateRequest& update_;
    Hooks hooks_;
    /// The tile the menu is open for.
    std::optional<library::ShelfItem> target_;
    std::string title_;
    std::vector<ui::SettingsRow> entries_;
};

} // namespace opensu::app
