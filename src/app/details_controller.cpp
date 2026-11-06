#include "details_controller.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "details_screen.hpp"
#include "library/emulator_choice.hpp"
#include "library/linked_play.hpp"
#include "library/rom_systems.hpp"
#include "library/wine.hpp"
#include "lucent/text.h"
#include "retro/save_paths.hpp"
#include "retro_launch.hpp"
#include "settings/video_settings.hpp"
#include "shader_preset_tree.hpp"
#include "ui/moment_text.hpp"

namespace opensu::app {
namespace {

std::string emulatorSourceText(library::EmulatorSource source, const library::Game& game) {
    switch (source) {
    case library::EmulatorSource::Game:
        return "Set for this game";
    case library::EmulatorSource::Romset:
        return "Automatic (romset)";
    case library::EmulatorSource::System:
        return "From " + library::roms::systemLabel(game.sourceId);
    case library::EmulatorSource::Search:
        break;
    }
    return "Automatic";
}

} // namespace

const gpu2d::ui::Screen* DetailsController::page() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const DetailsScreen*>(screen) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool DetailsController::isOpen() const {
    return page() != nullptr;
}

const ui::GameDetails* DetailsController::details() const {
    return isOpen() && details_ ? &*details_ : nullptr;
}

ui::GameSettingsView DetailsController::settingsOf(const library::Game& game) const {
    ui::GameSettingsView view;
    view.continues = hooks_.continues(game);
    view.installing = hooks_.installing(game);
    const settings::SettingLayers& layers = gameSettings_.layers();
    if (!game.emulatorOptions.empty()) {
        view.emulatorSource = emulatorSourceText(library::pickFor(layers, game).source, game);
    }
    const library::roms::EmulatorCatalog& catalog = downloads_.setup().catalog;
    const settings::LayerKeys keys = library::layerKeysOf(game);
    if (builtInOf(game, catalog) != nullptr) {
        const settings::Resolved shader = settings::resolveShader(layers, keys);
        for (const settings::ChoiceKey& video : settings::videoKeys) {
            const settings::Resolved resolved =
                layers.resolve(keys, video.key, std::string{video.choices[0].key});
            if (resolved.from) {
                view.facts.push_back(
                    ui::detailsFact(std::string{video.label},
                                    std::string{settings::choiceLabel(video, resolved.value)} +
                                        " · " + sourceText(resolved.from, catalog)));
            }
        }
        if (shader.from) {
            view.facts.push_back(ui::detailsFact(std::string{settings::shaderLabel},
                                                 presetLabel(shader.value) + " · " +
                                                     sourceText(shader.from, catalog)));
        }
    }
    if (const std::optional<std::size_t> kept = hooks_.gameSettings(game)) {
        const std::size_t custom = *kept;
        view.gameSettings = custom == 0 ? std::string{"Inherited"}
                                        : lucent::text::counted(custom, "custom setting");
    }
    if (const std::optional<retro::ResumeHeader> kept =
            game.source == library::Source::Rom ? hooks_.resume(game) : std::nullopt) {
        const library::roms::EmulatorEntry* runner = builtInOf(game, catalog);
        view.resume = runner != nullptr && retro::coreName(runner->core) == kept->core
                          ? "Resumes from " + ui::momentText(kept->saved)
                          : "Kept by " + (kept->coreLabel.empty() ? kept->core : kept->coreLabel) +
                                ", which " + game.title + " does not start on";
    }
    if (library::linkable(game)) {
        view.linked = true;
        view.partners = hooks_.partners();
    }
    view.online = library::netplayable(game);
    view.copySaves = hooks_.copySavesOffer(game);
    const SharedSaves::View shared = hooks_.sharedSaves(game);
    view.sharedSaves = shared.fact;
    view.syncSaves = shared.action;
    return view;
}

ui::GameDetails DetailsController::detailsOf(const library::Game& game) const {
    std::string download;
    const std::optional<EmulatorDownloads::Progress> progress = downloads_.progress();
    if (const library::EmulatorOption* missing = ui::emulatorToDownload(game);
        missing != nullptr && progress && progress->id == missing->id) {
        const std::optional<int> percent = progress->percent();
        download = percent ? std::to_string(*percent) + "% · A cancels" : "Starting · A cancels";
    }
    const library::wine::OwnPrefixes& own = preferences_.values().ownWinePrefixes;
    std::vector<ui::CopyPart> copies;
    for (library::Game& copy : copiesOf(game)) {
        const bool prefix = own.contains(copy.id);
        ui::GameSettingsView settings = settingsOf(copy);
        copies.push_back(ui::CopyPart{std::move(copy), prefix, std::move(settings)});
    }
    return ui::detailsOf(game,
                         ui::GameMarks{.hidden = preferences_.values().hidden.contains(game),
                                       .onHome = hooks_.onHome(game)},
                         std::chrono::system_clock::now(), download, own.contains(game.id),
                         settingsOf(game), copies);
}

void DetailsController::open() {
    const library::Game* game = hooks_.focused();
    if (game == nullptr) {
        return;
    }
    if (const gpu2d::ui::Screen* shown = page()) {
        navigator_.closeFrom(*shown);
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    openOrder_.clear();
    for (const library::Game& copy : hooks_.copies(*game)) {
        hooks_.readSaves(copy);
        openOrder_.push_back(copy.id);
    }
    if (openOrder_.empty()) {
        hooks_.readSaves(*game);
    }
    details_ = detailsOf(*game);
    navigator_.push(std::make_unique<DetailsScreen>(*this, chrome_, images_));
}

void DetailsController::close() {
    const gpu2d::ui::Screen* shown = page();
    if (shown == nullptr) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(*shown);
}

bool DetailsController::artDownloading() const {
    const ui::GameDetails* shown = details();
    return shown != nullptr && hooks_.artDownloading(shown->gameId);
}

void DetailsController::refresh() {
    if (!isOpen() || !details_) {
        return;
    }
    const std::optional<library::Game> game = hooks_.find(details_->gameId);
    if (!game) {
        navigator_.closeFrom(*page());
        return;
    }
    const bool downloading = ui::actionOf(*details_, ui::detailsDownloadId) != nullptr;
    details_ = detailsOf(*game);
    // The emulator arrived: Play is what the player came for.
    if (downloading && ui::actionOf(*details_, ui::detailsDownloadId) == nullptr &&
        navigator_.top() == page() && navigator_.focused() == ui::detailsDownloadId) {
        navigator_.focus(ui::detailsLaunchId);
    }
}

std::optional<library::Game> DetailsController::current() {
    if (!details_) {
        return std::nullopt;
    }
    std::optional<library::Game> game = hooks_.find(details_->gameId);
    if (!game) {
        close();
    }
    return game;
}

std::vector<library::Game> DetailsController::copiesOf(const library::Game& game) const {
    std::vector<library::Game> copies = hooks_.copies(game);
    std::ranges::stable_sort(copies, {}, [this](const library::Game& copy) {
        return std::ranges::find(openOrder_, copy.id) - openOrder_.begin();
    });
    return copies;
}

std::optional<library::Game> DetailsController::rowGame(const std::string& gameId) const {
    if (!details_) {
        return std::nullopt;
    }
    std::optional<library::Game> game = hooks_.find(details_->gameId);
    if (!game || gameId.empty() || gameId == game->id) {
        return game;
    }
    const std::vector<library::Game> copies = hooks_.copies(*game);
    const auto copy = std::ranges::find(copies, gameId, &library::Game::id);
    return copy == copies.end() ? std::nullopt : std::optional{*copy};
}

void DetailsController::launch(const std::string& id) {
    if (const std::optional<library::Game> game = rowGame(id)) {
        hooks_.launch(*game);
    }
}

void DetailsController::run(const std::string& rowId, int /*step*/) {
    const auto [id, gameId] = ui::copyRowOf(rowId);
    if (id == ui::detailsBackId) {
        close();
        return;
    }
    const std::optional<library::Game> game = rowGame(gameId);
    if (!game) {
        refresh();
        return;
    }
    if (id == ui::detailsLaunchId || id == ui::detailsInstallId) {
        hooks_.launch(*game);
    } else if (id == ui::detailsUpdateId) {
        update_.request(*game);
    } else if (id == ui::detailsDownloadId) {
        downloadEmulator(*game);
    } else if (id == ui::detailsWinePrefixId) {
        toggleOwnPrefix(*game);
    } else if (id == ui::detailsHomeId) {
        // Off Home, the page closes with the tile it was opened over (`refresh`).
        hooks_.setOnHome(*game, !hooks_.onHome(*game));
    } else if (id == ui::detailsHiddenId) {
        hooks_.setHidden(*game, !preferences_.values().hidden.contains(*game));
        // The game leaves or joins the shelf the page was opened over.
        close();
    } else if (id == ui::detailsUninstallId) {
        uninstall(*game);
    } else if (id == ui::detailsRenameId) {
        hooks_.rename(*game);
    } else if (id == ui::detailsChangeFileId) {
        hooks_.changeFile(*game);
    } else if (id == ui::detailsRemoveId) {
        hooks_.remove(*game);
    } else if (id == ui::detailsSyncSavesId) {
        hooks_.syncSaves(*game);
    } else if (id == ui::detailsLinkedId) {
        if (const std::optional<library::Game> linked = library::splitScreenGame(*game)) {
            hooks_.launch(*linked);
        }
    } else if (id.starts_with(ui::detailsPlayWithPrefix)) {
        hooks_.playWith(*game, id.substr(ui::detailsPlayWithPrefix.size()));
    } else if (id == ui::detailsPlayOnlineId) {
        hooks_.playOnline(*game);
    } else if (id == ui::detailsStartFreshId) {
        if (const std::string refused = hooks_.startFresh(*game); !refused.empty()) {
            hooks_.say(refused, true);
            refresh();
            return;
        }
        hooks_.launch(*game);
    }
}

void DetailsController::openPage(const std::string& rowId) {
    const auto [id, gameId] = ui::copyRowOf(rowId);
    const std::optional<library::Game> game = rowGame(gameId);
    if (!game) {
        refresh();
        return;
    }
    if (id == ui::detailsMoreId) {
        // input-sound.md 3.4 Open: a panel appears.
        sounds_.play(audio::Effect::Open);
        navigator_.push(std::make_unique<DetailsMoreScreen>(*this, chrome_));
    } else if (id == ui::detailsSettingsId) {
        hooks_.openGameSettings(*game, [this] {
            return details_ ? details_->settingsRows : std::vector<ui::SettingsRow>{};
        });
    } else if (id == ui::detailsArtworkId) {
        hooks_.chooseArtwork(*game);
    } else if (id == ui::detailsControlsId) {
        hooks_.controls(*game);
    } else if (id == ui::detailsMatchId) {
        hooks_.matchGame(*game);
    } else if (id == ui::detailsSavesId) {
        hooks_.copySaves(*game);
    }
}

void DetailsController::downloadEmulator(const library::Game& game) {
    const library::EmulatorOption* missing = ui::emulatorToDownload(game);
    if (missing == nullptr) {
        return;
    }
    if (const std::optional<EmulatorDownloads::Progress> progress = downloads_.progress();
        progress && progress->id == missing->id) {
        downloads_.cancel(missing->id);
    } else if (const std::string refused = downloads_.start(missing->id); !refused.empty()) {
        hooks_.say(refused, true);
    }
    refresh();
}

void DetailsController::toggleOwnPrefix(const library::Game& game) {
    library::wine::OwnPrefixes& own = preferences_.values().ownWinePrefixes;
    const bool nowOwn = !own.contains(game.id);
    own.set(game.id, nowOwn);
    preferences_.save();
    hooks_.say(game.title + (nowOwn ? " gets a Wine prefix of its own"
                                    : " shares the Wine prefix again; its own is kept"),
               false);
    refresh();
}

void DetailsController::uninstall(const library::Game& game) {
    const std::optional<std::string> what = uninstall_.removal(game);
    if (!what) {
        return;
    }
    navigator_.push(gpu2d::ui::confirm(
        UninstallPrompt::question(game), *what, "Uninstall", [this, id = game.id] {
            if (const std::optional<library::Game> gone = rowGame(id)) {
                uninstall_.start(*gone);
            }
        }));
}

} // namespace opensu::app
