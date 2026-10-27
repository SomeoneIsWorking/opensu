#include "details_controller.hpp"

#include <algorithm>
#include <chrono>
#include <string>

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
                view.rows.push_back(
                    ui::DetailsRow{std::string{video.label},
                                   std::string{settings::choiceLabel(video, resolved.value)} +
                                       " · " + sourceText(resolved.from, catalog)});
            }
        }
        if (shader.from) {
            view.rows.push_back(ui::DetailsRow{std::string{settings::shaderLabel},
                                               presetLabel(shader.value) + " · " +
                                                   sourceText(shader.from, catalog)});
        }
    }
    if (const std::optional<std::size_t> kept = hooks_.gameSettings(game)) {
        const std::size_t custom = *kept;
        view.buttons.push_back(ui::DetailsButton{
            ui::DetailsAction::GameSettings, "Game settings",
            custom == 0 ? std::string{"Inherited"}
                        : std::to_string(custom) + " custom setting" + (custom == 1 ? "" : "s")});
    }
    return view;
}

ui::DetailsView DetailsController::viewOf(const library::Game& game) const {
    std::string download;
    const std::optional<EmulatorDownloads::Progress> progress = downloads_.progress();
    if (const library::EmulatorOption* missing = ui::emulatorToDownload(game);
        missing != nullptr && progress && progress->id == missing->id) {
        const std::optional<int> percent = progress->percent();
        download = percent ? std::to_string(*percent) + "% · A cancels" : "Starting · A cancels";
    }
    return ui::detailsFor(
        game, preferences_.values().hidden.contains(game), std::chrono::system_clock::now(),
        download, preferences_.values().ownWinePrefixes.contains(game.id), settingsOf(game));
}

void DetailsController::open() {
    const library::Game* game = hooks_.focused();
    if (game == nullptr) {
        return;
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    page_.open(viewOf(*game));
}

void DetailsController::close() {
    if (!page_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    page_.close();
}

void DetailsController::refresh() {
    if (!page_.isOpen()) {
        return;
    }
    if (const std::optional<library::Game> game = hooks_.find(page_.view().gameId)) {
        page_.refresh(viewOf(*game));
    } else {
        page_.close();
    }
}

void DetailsController::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (page_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        if (page_.selected() == ui::DetailsAction::Emulator) {
            if (const std::optional<library::Game> game = hooks_.find(page_.view().gameId)) {
                cycleEmulator(*game, button == gamepad::Button::Right ? 1 : -1);
            }
        }
        break;
    case gamepad::Button::A:
        run(page_.selected());
        break;
    case gamepad::Button::B:
        close();
        break;
    default:
        break;
    }
}

void DetailsController::run(ui::DetailsAction action) {
    if (action == ui::DetailsAction::Back) {
        close();
        return;
    }
    const std::optional<library::Game> game = hooks_.find(page_.view().gameId);
    if (!game) {
        close();
        return;
    }
    switch (action) {
    case ui::DetailsAction::Launch:
        hooks_.launch(*game);
        break;
    case ui::DetailsAction::Update:
        sounds_.play(audio::Effect::Navigation);
        update_.request(*game);
        break;
    case ui::DetailsAction::Emulator:
        cycleEmulator(*game, 1);
        break;
    case ui::DetailsAction::GameSettings:
        hooks_.openGameSettings(*game);
        break;
    case ui::DetailsAction::DownloadEmulator:
        downloadEmulator(*game);
        break;
    case ui::DetailsAction::WinePrefix:
        toggleOwnPrefix(*game);
        break;
    case ui::DetailsAction::Hidden:
        hooks_.setHidden(*game, !preferences_.values().hidden.contains(*game));
        // The game leaves or joins the shelf the page was opened over.
        close();
        break;
    case ui::DetailsAction::Uninstall:
        uninstall(*game);
        break;
    case ui::DetailsAction::Controls:
        hooks_.controls(*game);
        break;
    case ui::DetailsAction::ChooseArtwork:
        hooks_.chooseArtwork(*game);
        break;
    case ui::DetailsAction::Back:
        break;
    }
}

void DetailsController::downloadEmulator(const library::Game& game) {
    const library::EmulatorOption* missing = ui::emulatorToDownload(game);
    if (missing == nullptr) {
        return;
    }
    sounds_.play(audio::Effect::Navigation);
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
    sounds_.play(audio::Effect::Navigation);
    hooks_.say(game.title + (nowOwn ? " gets a Wine prefix of its own"
                                    : " shares the Wine prefix again; its own is kept"),
               false);
    refresh();
}

void DetailsController::uninstall(const library::Game& game) {
    if (page_.armed()) {
        page_.disarm();
        uninstall_.confirm(game);
        return;
    }
    if (std::optional<std::string> what = uninstall_.arm(game)) {
        // input-sound.md 3.4 Navigation: the button changes in place.
        sounds_.play(audio::Effect::Navigation);
        page_.arm(std::move(*what));
    }
}

void DetailsController::cycleEmulator(const library::Game& game, int delta) {
    const auto& options = game.emulatorOptions;
    if (options.size() < 2) {
        return;
    }
    const auto at = std::ranges::find(options, game.emulator, &library::EmulatorOption::name);
    const auto from = at == options.end() ? 0 : static_cast<int>(at - options.begin());
    const auto count = static_cast<int>(options.size());
    const auto next = static_cast<std::size_t>((from + delta + count) % count);
    sounds_.play(audio::Effect::Navigation);
    hooks_.chooseEmulator(game, options[next].name);
}

} // namespace opensu::app
