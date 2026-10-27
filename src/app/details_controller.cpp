#include "details_controller.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "details_screen.hpp"
#include "library/emulator_choice.hpp"
#include "library/rom_systems.hpp"
#include "library/wine.hpp"
#include "retro_launch.hpp"
#include "settings/video_settings.hpp"
#include "shader_preset_tree.hpp"

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
        view.gameSettings =
            custom == 0 ? std::string{"Inherited"}
                        : std::to_string(custom) + " custom setting" + (custom == 1 ? "" : "s");
    }
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
    return ui::detailsOf(game, preferences_.values().hidden.contains(game),
                         std::chrono::system_clock::now(), download,
                         preferences_.values().ownWinePrefixes.contains(game.id), settingsOf(game));
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

void DetailsController::launch() {
    if (const std::optional<library::Game> game = current()) {
        hooks_.launch(*game);
    }
}

void DetailsController::run(const std::string& id, int step) {
    if (id == ui::detailsBackId) {
        close();
        return;
    }
    const std::optional<library::Game> game = current();
    if (!game) {
        return;
    }
    if (id == ui::detailsUpdateId) {
        update_.request(*game);
    } else if (id == ui::detailsDownloadId) {
        downloadEmulator(*game);
    } else if (id == ui::detailsEmulatorId) {
        chooseEmulator(*game, step);
    } else if (id == ui::detailsWinePrefixId) {
        toggleOwnPrefix(*game);
    } else if (id == ui::detailsHiddenId) {
        hooks_.setHidden(*game, !preferences_.values().hidden.contains(*game));
        // The game leaves or joins the shelf the page was opened over.
        close();
    } else if (id == ui::detailsUninstallId) {
        uninstall(*game);
    }
}

void DetailsController::openPage(const std::string& id) {
    const std::optional<library::Game> game = current();
    if (!game) {
        return;
    }
    if (id == ui::detailsSettingsId) {
        hooks_.openGameSettings(*game);
    } else if (id == ui::detailsArtworkId) {
        hooks_.chooseArtwork(*game);
    } else if (id == ui::detailsControlsId) {
        hooks_.controls(*game);
    }
}

void DetailsController::chooseEmulator(const library::Game& game, int step) {
    if (!details_) {
        return;
    }
    const ui::SettingsRow* row = ui::actionOf(*details_, ui::detailsEmulatorId);
    if (row == nullptr || row->choices.empty()) {
        return;
    }
    const auto count = static_cast<int>(row->choices.size());
    const auto next =
        static_cast<std::size_t>(((static_cast<int>(row->chosen) + step) % count + count) % count);
    const std::string& name = row->choices[next];
    if (name != ui::noEmulator) {
        hooks_.chooseEmulator(game, name);
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
        "Uninstall " + game.title + "?", *what, "Uninstall", [this, id = game.id] {
            if (const std::optional<library::Game> gone = hooks_.find(id)) {
                uninstall_.start(*gone);
            }
        }));
}

} // namespace opensu::app
