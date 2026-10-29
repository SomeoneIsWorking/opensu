#include "context_menu_controller.hpp"

#include <algorithm>
#include <memory>
#include <variant>

#include "context_menu_screen.hpp"
#include "gpu2d/ui/screen.hpp"

namespace opensu::app {
namespace {

/// Whether a launcher tile for a store with its own sign-in has none yet.
bool needsSignIn(const library::Launcher& launcher) noexcept {
    return !launcher.ready && !launcher.loading &&
           (launcher.source == library::Source::Gog || launcher.source == library::Source::Epic);
}

ui::SettingsRow entry(std::string_view id, std::string label) {
    return ui::makeRow(std::string{id}, ui::RowKind::Action, std::move(label), {}, {});
}

} // namespace

std::vector<ui::SettingsRow> contextEntriesFor(const library::ShelfItem& item, bool hidden) {
    std::vector<ui::SettingsRow> entries;
    if (const auto* game = std::get_if<library::Game>(&item)) {
        entries.push_back(game->installed ? entry(menuLaunchId, "Launch")
                                          : entry(menuInstallId, "Install"));
        if (game->installed && game->updateAvailable && game->source != library::Source::Rom) {
            entries.push_back(entry(menuUpdateId, "Update"));
        }
        entries.push_back(entry(menuDetailsId, "Details"));
        entries.push_back(hidden ? entry(menuUnhideId, "Unhide game")
                                 : entry(menuHideId, "Hide game"));
        if (game->installed && game->source != library::Source::Rom) {
            entries.push_back(entry(menuUninstallId, "Uninstall"));
        }
        return entries;
    }
    entries.push_back(entry(menuOpenId, "Open"));
    if (const auto* launcher = std::get_if<library::Launcher>(&item);
        launcher != nullptr && needsSignIn(*launcher)) {
        entries.push_back(entry(menuSignInId, "Sign in"));
    }
    entries.push_back(entry(menuRefreshId, "Refresh library"));
    return entries;
}

const gpu2d::ui::Screen* ContextMenuController::menu() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const ContextMenuScreen*>(screen) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool ContextMenuController::isOpen() const {
    return menu() != nullptr;
}

void ContextMenuController::open() {
    const library::ShelfItem* focused = hooks_.focused();
    if (focused == nullptr || isOpen()) {
        return;
    }
    target_ = *focused;
    bool hidden = false;
    title_.clear();
    if (const auto* game = std::get_if<library::Game>(&*target_)) {
        title_ = game->title;
        hidden = preferences_.values().hidden.contains(*game);
    } else if (const std::optional<library::Folder> folder = library::folderOf(*target_)) {
        title_ = library::name(*folder);
    }
    entries_ = contextEntriesFor(*target_, hidden);
    // input-sound.md 3.4 OpenContextMenu: the home context menu becomes visible.
    sounds_.play(audio::Effect::OpenContextMenu);
    navigator_.push(std::make_unique<ContextMenuScreen>(*this));
}

void ContextMenuController::close() {
    const gpu2d::ui::Screen* open = menu();
    if (open == nullptr) {
        return;
    }
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(*open);
}

void ContextMenuController::run(const std::string& id, int /*step*/) {
    const gpu2d::ui::Screen* open = menu();
    if (open == nullptr || !target_) {
        return;
    }
    navigator_.closeFrom(*open);
    const library::ShelfItem target = *target_;
    const auto* game = std::get_if<library::Game>(&target);
    if (id == menuLaunchId || id == menuInstallId) {
        hooks_.launch();
    } else if (id == menuDetailsId) {
        hooks_.details();
    } else if (id == menuRefreshId) {
        hooks_.refresh();
    } else if (id == menuOpenId) {
        if (const std::optional<library::Folder> folder = library::folderOf(target)) {
            hooks_.open(*folder);
        }
    } else if (id == menuSignInId) {
        if (const auto* launcher = std::get_if<library::Launcher>(&target)) {
            hooks_.signIn(launcher->source);
        }
    } else if (game == nullptr) {
        return;
    } else if (id == menuUpdateId) {
        update_.request(*game);
    } else if (id == menuHideId || id == menuUnhideId) {
        hooks_.setHidden(*game, id == menuHideId);
    } else if (id == menuUninstallId) {
        uninstall(*game);
    }
}

void ContextMenuController::uninstall(const library::Game& game) {
    const std::optional<std::string> what = uninstall_.removal(game);
    if (!what) {
        return;
    }
    navigator_.push(gpu2d::ui::confirm("Uninstall " + game.title + "?", *what, "Uninstall",
                                       [this, game = game] {
                                           uninstall_.start(game);
                                       }));
}

} // namespace opensu::app
