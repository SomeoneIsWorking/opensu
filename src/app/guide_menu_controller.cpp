#include "guide_menu_controller.hpp"

#include <algorithm>
#include <memory>

#include "guide_menu_screen.hpp"
#include "settings/settings.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;
using ui::SettingsRow;

constexpr const char* resumeId = "guide.resume";
constexpr const char* closeGameId = "guide.closeGame";
constexpr const char* gameSettingsId = "guide.gameSettings";
constexpr const char* saveStateId = "guide.saveState";
constexpr const char* loadStateId = "guide.loadState";
constexpr const char* resetGameId = "guide.resetGame";
constexpr const char* homeId = "guide.home";
constexpr const char* libraryId = "guide.library";
constexpr const char* devicesId = "guide.devices";
constexpr const char* downloadsId = "guide.downloads";
constexpr const char* settingsId = "guide.settings";
constexpr const char* powerId = "guide.power";
constexpr const char* volumeId = "guide.volume";
constexpr const char* muteId = "guide.mute";
constexpr const char* brightnessId = "guide.brightness";
constexpr const char* scaleId = "guide.scale";
constexpr const char* sleepId = "power.sleep";
constexpr const char* restartId = "power.restart";
constexpr const char* shutDownId = "power.shutDown";
constexpr const char* toDesktopId = "power.toDesktop";
constexpr const char* toSessionId = "power.toSession";
constexpr const char* installSessionId = "power.installSession";
constexpr const char* quitId = "power.quit";

SettingsRow action(std::string id, std::string label, std::string note = {}) {
    return ui::makeRow(std::move(id), RowKind::Action, std::move(label), std::move(note), {});
}

/// An action that asks first, saying `what` it throws away; `verb` is its button.
SettingsRow asking(std::string id, std::string label, std::string what, std::string verb = {}) {
    SettingsRow row = action(std::move(id), std::move(label));
    row.confirm = std::move(what);
    row.confirmVerb = std::move(verb);
    return row;
}

std::string percentOf(int percent) {
    return std::to_string(percent) + "%";
}

} // namespace

const gpu2d::ui::Screen* GuideMenuController::menu() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const GuideMenuScreen*>(screen) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool GuideMenuController::isOpen() const {
    return menu() != nullptr;
}

void GuideMenuController::toggle(Landing landing) {
    if (isOpen()) {
        close();
        return;
    }
    // input-sound.md 3.4 OpenContextMenu: a menu becomes visible (r90.java).
    sounds_.play(audio::Effect::OpenContextMenu);
    context_ = hooks_.context();
    read();
    navigator_.push(std::make_unique<GuideMenuScreen>(*this, landing));
}

void GuideMenuController::close() {
    const gpu2d::ui::Screen* open = menu();
    if (open == nullptr) {
        return;
    }
    // input-sound.md 3.4 Close: a menu becomes hidden.
    sounds_.play(audio::Effect::Close);
    navigator_.closeFrom(*open);
}

void GuideMenuController::read() {
    retroStatus_ = context_.inGame ? retro_.status() : std::nullopt;
    brightnessLevel_ = brightness_.percent();
}

std::string GuideMenuController::title() const {
    return context_.inGame ? context_.title : std::string{"openSU"};
}

std::vector<SettingsRow> GuideMenuController::places() const {
    std::vector<SettingsRow> rows;
    if (context_.inGame) {
        rows.push_back(action(resumeId, "Resume"));
        rows.push_back(asking(closeGameId, "Close game",
                              "The game ends; anything it has not saved is lost.", "Close"));
        if (context_.gameSettings) {
            rows.push_back(ui::makePage(gameSettingsId, "Game settings", {}, {}));
        }
        if (retroStatus_) {
            rows.push_back(action(saveStateId, "Save state"));
            if (retroStatus_->hasState) {
                rows.push_back(asking(loadStateId, "Load state",
                                      "The game goes back to the state saved last; play since "
                                      "then is lost.",
                                      "Load"));
            } else {
                rows.push_back(
                    ui::makeRow(loadStateId, RowKind::Info, "Load state", "No state saved", {}));
            }
            rows.push_back(asking(resetGameId, "Reset game",
                                  "The game starts over; play it has not saved is lost.", "Reset"));
        }
        rows.push_back(ui::makeHeading("openSU"));
    } else {
        rows.push_back(action(homeId, "Home"));
        rows.push_back(action(libraryId, "Library"));
    }
    rows.push_back(action(devicesId, "Devices"));
    rows.push_back(action(downloadsId, "Downloads"));
    rows.push_back(action(settingsId, "Settings"));
    rows.push_back(ui::makePage(powerId, "Power", {}, {}));
    return rows;
}

std::vector<SettingsRow> GuideMenuController::quick() const {
    std::vector<SettingsRow> rows;
    const audio::SystemVolume& volume = volume_.system();
    if (volume.available()) {
        const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
        rows.push_back(ui::makeSlider(volumeId, "Volume", {},
                                      ui::SliderRange{state.percent, 0, 100, audio::volumeStep}));
        rows.push_back(ui::makeToggle(muteId, "Mute", {}, state.muted));
    } else {
        rows.push_back(ui::makeRow("guide.volumeMissing", RowKind::Info, "Volume",
                                   volume.unavailable(), "Not available"));
    }
    if (brightnessLevel_) {
        rows.push_back(ui::makeSlider(brightnessId, "Brightness", {},
                                      ui::SliderRange{*brightnessLevel_, 1, 100, brightnessStep}));
    }
    const int scale = preferences_.values().uiScale;
    ui::Options scales;
    for (int percent = settings::minUiScale; percent <= settings::maxUiScale;
         percent += settings::uiScaleStep) {
        if (percent <= scale) {
            scales.chosen = scales.labels.size();
        }
        scales.labels.push_back(percentOf(percent));
    }
    rows.push_back(ui::makeChoice(scaleId, "Interface size", {}, std::move(scales)));
    return rows;
}

std::vector<SettingsRow> GuideMenuController::power() const {
    std::vector<SettingsRow> rows;
    rows.push_back(action(sleepId, "Sleep"));
    rows.push_back(asking(restartId, "Restart", "This computer restarts; a running game ends."));
    rows.push_back(asking(shutDownId, "Shut down", "This computer turns off; a running game ends.",
                          "Shut down"));
    // As the login session, quitting only ends it and the display manager starts it again.
    if (context_.loginSession) {
        rows.push_back(action(toDesktopId, "Switch to desktop"));
        return rows;
    }
    if (context_.sessionInstalled) {
        rows.push_back(asking(toSessionId, "Switch to session mode",
                              "Your desktop session ends and openSU's starts; a running game "
                              "ends."));
    } else {
        rows.push_back(action(installSessionId, "Install session mode"));
    }
    rows.push_back(action(quitId, "Quit to desktop"));
    return rows;
}

void GuideMenuController::leave(const std::function<void()>& then) {
    close();
    then();
}

void GuideMenuController::refuse(const std::string& reason, const std::string& what) {
    if (!reason.empty()) {
        hooks_.say(what.empty() ? reason : what + ": " + reason, true);
    }
}

void GuideMenuController::run(const std::string& id, int step) {
    if (id == muteId) {
        refuse(volume_.system().toggleMute());
    } else if (id == scaleId) {
        settings::Settings& values = preferences_.values();
        values.uiScale = settings::steppedUiScale(values.uiScale, step);
        preferences_.save();
        hooks_.applyLayout();
    } else if (id == resumeId) {
        close();
    } else if (id == closeGameId || id == saveStateId || id == loadStateId || id == resetGameId) {
        runGame(id);
    } else if (id == homeId) {
        leave([this] {
            hooks_.showSection(library::Section::Home);
        });
    } else if (id == libraryId) {
        leave([this] {
            hooks_.showSection(library::Section::Library);
        });
    } else if (id == devicesId) {
        leave(hooks_.openDevices);
    } else if (id == downloadsId) {
        leave(hooks_.openDownloads);
    } else if (id == settingsId) {
        leave(hooks_.openSettings);
    } else {
        runPower(id);
    }
}

void GuideMenuController::runGame(const std::string& id) {
    if (id == closeGameId) {
        leave(hooks_.closeGame);
        return;
    }
    const RetroSession::Command command = id == saveStateId   ? RetroSession::Command::SaveState
                                          : id == loadStateId ? RetroSession::Command::LoadState
                                                              : RetroSession::Command::Reset;
    const RetroSession::Outcome outcome = retro_.perform(command);
    hooks_.say(outcome.text, outcome.error);
    read();
}

void GuideMenuController::runPower(const std::string& id) {
    if (id == sleepId || id == restartId || id == shutDownId) {
        const host::PowerAction what = id == sleepId     ? host::PowerAction::Suspend
                                       : id == restartId ? host::PowerAction::Restart
                                                         : host::PowerAction::ShutDown;
        close();
        refuse(power_.perform(what), host::label(what));
    } else if (id == toDesktopId) {
        close();
        refuse(session_.switchToDesktop(), "Switch to desktop");
    } else if (id == toSessionId) {
        close();
        refuse(session_.switchToSession(), "Switch to session mode");
    } else if (id == installSessionId) {
        leave(hooks_.installSession);
    } else if (id == quitId) {
        leave(hooks_.quit);
    }
}

void GuideMenuController::setLevel(const std::string& id, int level) {
    if (id == volumeId) {
        refuse(volume_.system().setPercent(std::clamp(level, 0, 100)));
    } else if (id == brightnessId) {
        refuse(brightness_.set(level));
        brightnessLevel_ = brightness_.percent();
    }
}

void GuideMenuController::openPage(const std::string& id) {
    if (id == gameSettingsId) {
        hooks_.openGameSettings();
    } else if (id == powerId) {
        navigator_.push(std::make_unique<GuidePowerScreen>(*this));
    }
}

} // namespace opensu::app
