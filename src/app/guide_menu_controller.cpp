#include "guide_menu_controller.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>

#include "guide_menu_screen.hpp"
#include "settings/settings.hpp"
#include "ui/moment_text.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;
using ui::SettingsRow;

constexpr const char* resumeId = "guide.resume";
constexpr const char* closeGameId = "guide.closeGame";
constexpr const char* saveAndCloseId = "guide.saveAndClose";
constexpr const char* resumedId = "guide.resumed";
constexpr const char* gameSettingsId = "guide.gameSettings";
constexpr std::string_view inviteId = "guide.invite";
constexpr std::string_view playersId = "guide.players";
constexpr std::string_view invitePrefix = "guide.invite/";
constexpr std::string_view kickPrefix = "guide.kick/";
constexpr const char* saveStateId = "guide.saveState";
constexpr const char* loadStateId = "guide.loadState";
constexpr const char* resetGameId = "guide.resetGame";
constexpr const char* homeId = "guide.home";
constexpr const char* libraryId = "guide.library";
constexpr const char* searchId = "guide.search";
constexpr const char* devicesId = "guide.devices";
constexpr const char* downloadsId = "guide.downloads";
constexpr const char* settingsId = "guide.settings";
constexpr const char* powerId = "guide.power";
constexpr const char* volumeId = "guide.volume";
constexpr const char* muteId = "guide.mute";
constexpr const char* brightnessId = "guide.brightness";
constexpr const char* scaleId = "guide.scale";
constexpr const char* profileId = "guide.profile";
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
    retroStatus_ = context_.inGame && !context_.remote ? retro_.status() : std::nullopt;
    brightnessLevel_ = brightness_.percent();
}

std::string GuideMenuController::title() const {
    return context_.inGame ? context_.title : std::string{"openSU"};
}

std::vector<SettingsRow> GuideMenuController::places() const {
    std::vector<SettingsRow> rows;
    if (context_.inGame) {
        rows.push_back(action(resumeId, "Resume"));
        if (context_.remote && context_.joined) {
            rows.push_back(asking(closeGameId, "Leave game",
                                  "You leave the game; it goes on on the openSU it runs on.",
                                  "Leave"));
        } else if (context_.remote) {
            rows.push_back(asking(closeGameId, "End remote play",
                                  "The game closes on the openSU it runs on; anything it has not "
                                  "saved is lost.",
                                  "End"));
        } else {
            rows.push_back(asking(closeGameId, "Close game",
                                  "The game ends; anything it has not saved is lost.", "Close"));
        }
        if (retroStatus_) {
            rows.push_back(action(saveAndCloseId, "Save and close"));
        }
        if (context_.takesGuests) {
            rows.push_back(ui::makePage(std::string{inviteId}, "Invite to play", {}, {}));
        }
        if (!context_.guests.empty()) {
            rows.push_back(ui::makePage(std::string{playersId}, "Players", {},
                                        std::to_string(context_.guests.size())));
        }
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
            if (retroStatus_->resumedFrom) {
                rows.push_back(ui::makeRow(resumedId, RowKind::Info, "Resumed session",
                                           ui::momentText(*retroStatus_->resumedFrom), {}));
            } else if (!retroStatus_->resumeRefused.empty()) {
                rows.push_back(ui::makeRow(resumedId, RowKind::Info, "Not resumed",
                                           retroStatus_->resumeRefused, {}));
            }
        }
        rows.push_back(ui::makeHeading("openSU"));
    } else {
        rows.push_back(action(homeId, "Home"));
        rows.push_back(action(libraryId, "Library"));
    }
    if (context_.page) {
        rows.push_back(action(searchId, "Search"));
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
    const std::string active = profiles_.active().id;
    ui::Options players;
    for (const profiles::Profile& profile : profiles_.all()) {
        if (profile.id == active) {
            players.chosen = players.labels.size();
        }
        players.labels.push_back(profile.name);
    }
    rows.push_back(ui::makeChoice(profileId, "Profile", {}, std::move(players)));
    if (context_.page) {
        rows.push_back(
            ui::makeHeading(*context_.page == library::Section::Home ? "Home" : "Library"));
        std::ranges::move(shelf_.rowsFor(*context_.page), std::back_inserter(rows));
    }
    return rows;
}

std::vector<SettingsRow> GuideMenuController::power() const {
    std::vector<SettingsRow> rows;
    if (power_ != nullptr) {
        rows.push_back(action(sleepId, "Sleep"));
        rows.push_back(
            asking(restartId, "Restart", "This computer restarts; a running game ends."));
        rows.push_back(asking(shutDownId, "Shut down",
                              "This computer turns off; a running game ends.", "Shut down"));
    }
    // As the login session, quitting only ends it and the display manager starts it again.
    if (session_ != nullptr && context_.loginSession) {
        rows.push_back(action(toDesktopId, "Switch to desktop"));
        return rows;
    }
    if (session_ != nullptr && context_.sessionInstalled) {
        rows.push_back(asking(toSessionId, "Switch to session mode",
                              "Your desktop session ends and openSU's starts; a running game "
                              "ends."));
    } else if (session_ != nullptr) {
        rows.push_back(action(installSessionId, "Install session mode"));
    }
    rows.push_back(action(quitId, "Quit to desktop"));
    return rows;
}

std::vector<SettingsRow> GuideMenuController::invitable() const {
    std::vector<SettingsRow> rows;
    rows.reserve(context_.invitable.size() + 1);
    for (const lan::Peer& peer : context_.invitable) {
        rows.push_back(action(std::string{invitePrefix} + peer.id, peer.name));
    }
    if (rows.empty()) {
        rows.push_back(ui::makeRow("guide.inviteNone", RowKind::Info, "No one to invite",
                                   "Paired openSUs on the LAN show here.", {}));
    }
    return rows;
}

std::vector<SettingsRow> GuideMenuController::players() const {
    std::vector<SettingsRow> rows;
    rows.reserve(context_.guests.size() + 1);
    for (const lan::Peer& peer : context_.guests) {
        rows.push_back(asking(std::string{kickPrefix} + peer.id, "Kick " + peer.name,
                              context_.closesWithGuest
                                  ? peer.name +
                                        " leaves and the game closes, as it was started for them; "
                                        "anything it has not saved is lost."
                                  : peer.name + " leaves the game; it goes on here.",
                              "Kick"));
    }
    if (rows.empty()) {
        rows.push_back(ui::makeRow("guide.playersNone", RowKind::Info, "No guests", {}, {}));
    }
    return rows;
}

void GuideMenuController::closePage(std::string_view id) {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == id) {
            navigator_.closeFrom(*screen);
            return;
        }
    }
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
    if (shelf_.run(id, step)) {
        return;
    }
    if (id == muteId) {
        refuse(volume_.system().toggleMute());
    } else if (id == scaleId) {
        settings::Settings& values = preferences_.values();
        values.uiScale = settings::steppedUiScale(values.uiScale, step);
        preferences_.save();
        hooks_.applyLayout();
    } else if (id == profileId) {
        stepProfile(step);
    } else if (id == resumeId) {
        close();
    } else if (id == closeGameId || id == saveAndCloseId || id == saveStateId ||
               id == loadStateId || id == resetGameId) {
        runGame(id);
    } else if (id == homeId) {
        leave([this] {
            hooks_.showSection(library::Section::Home);
        });
    } else if (id == libraryId) {
        leave([this] {
            hooks_.showSection(library::Section::Library);
        });
    } else if (id == searchId) {
        leave(hooks_.openSearch);
    } else if (id == devicesId) {
        leave(hooks_.openDevices);
    } else if (id == downloadsId) {
        leave(hooks_.openDownloads);
    } else if (id == settingsId) {
        leave(hooks_.openSettings);
    } else if (id.starts_with(invitePrefix) || id.starts_with(kickPrefix)) {
        runTogether(id);
    } else {
        runPower(id);
    }
}

void GuideMenuController::runGame(const std::string& id) {
    if (id == closeGameId) {
        leave(hooks_.closeGame);
        return;
    }
    if (id == saveAndCloseId) {
        if (const std::string refused = retro_.suspend(); !refused.empty()) {
            hooks_.say("Not saved: " + refused, true);
            read();
            return;
        }
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

void GuideMenuController::runTogether(const std::string& id) {
    if (id.starts_with(kickPrefix)) {
        hooks_.kick(id.substr(kickPrefix.size()));
        close();
        return;
    }
    const std::string peerId = id.substr(invitePrefix.size());
    const auto peer = std::ranges::find(context_.invitable, peerId, &lan::Peer::id);
    if (peer != context_.invitable.end()) {
        hooks_.invite(*peer);
    }
    closePage(inviteId);
}

void GuideMenuController::runPower(const std::string& id) {
    if (power_ != nullptr && (id == sleepId || id == restartId || id == shutDownId)) {
        const host::PowerAction what = id == sleepId     ? host::PowerAction::Suspend
                                       : id == restartId ? host::PowerAction::Restart
                                                         : host::PowerAction::ShutDown;
        close();
        refuse(power_->perform(what), host::label(what));
    } else if (session_ != nullptr && id == toDesktopId) {
        close();
        refuse(session_->switchToDesktop(), "Switch to desktop");
    } else if (session_ != nullptr && id == toSessionId) {
        close();
        refuse(session_->switchToSession(), "Switch to session mode");
    } else if (id == installSessionId) {
        leave(hooks_.installSession);
    } else if (id == quitId) {
        leave(hooks_.quit);
    }
}

void GuideMenuController::stepProfile(int step) {
    const std::vector<profiles::Profile> all = profiles_.all();
    const std::string active = profiles_.active().id;
    const auto at = std::ranges::find(all, active, &profiles::Profile::id);
    if (all.size() < 2 || at == all.end() || step == 0) {
        return;
    }
    const auto count = static_cast<std::ptrdiff_t>(all.size());
    const std::ptrdiff_t next = ((at - all.begin() + step) % count + count) % count;
    hooks_.playAs(all[static_cast<std::size_t>(next)].id);
}

void GuideMenuController::setLevel(const std::string& id, int level) {
    if (shelf_.setLevel(id, level)) {
        return;
    }
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
        navigator_.push(std::make_unique<GuideListScreen>(*this, powerId, "Power", [this] {
            return power();
        }));
    } else if (id == inviteId) {
        context_ = hooks_.context();
        navigator_.push(std::make_unique<GuideListScreen>(*this, id, "Invite to play", [this] {
            return invitable();
        }));
    } else if (id == playersId) {
        context_ = hooks_.context();
        navigator_.push(std::make_unique<GuideListScreen>(*this, id, "Players", [this] {
            return players();
        }));
    }
}

} // namespace opensu::app
