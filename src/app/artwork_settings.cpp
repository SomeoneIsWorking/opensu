#include "artwork_settings.hpp"

#include "keyboard_pad.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;
using State = SteamGridDbAccount::State;

constexpr const char* statusId = "artwork.steamgriddb";
constexpr const char* connectId = "artwork.steamgriddb.connect";
constexpr const char* cancelId = "artwork.steamgriddb.cancel";
constexpr const char* enterId = "artwork.steamgriddb.enter";
constexpr const char* disconnectId = "artwork.steamgriddb.disconnect";

std::string statusText(const SteamGridDbAccount::Status& status) {
    switch (status.state) {
    case State::Loading:
        return "Reading the saved key";
    case State::Disconnected:
        return "Not connected";
    case State::StartingSteam:
        return "Starting Steam";
    case State::OpeningSteam:
        return "Opening SteamGridDB in Steam";
    case State::WaitingForPlayer:
        return "Confirm the sign-in on Steam's page";
    case State::ReadingKey:
        return "Reading the API key";
    case State::Connected:
        return status.refused ? "SteamGridDB refused the key" : "Connected";
    }
    return {};
}

} // namespace

ui::SettingsCategory ArtworkSettings::page() const {
    const SteamGridDbAccount::Status status = account_.status();
    ui::SettingsCategory page{std::string{categoryId}, "Artwork", {}};
    std::string note = "Covers that fit their tiles";
    if (status.state == State::Connected) {
        note = status.place == host::SecretPlace::Keyring ? "Key kept in the keyring"
                                                          : "Key kept in a private file";
    }
    page.rows.push_back(
        ui::makeRow(statusId, RowKind::Info, "SteamGridDB", note, statusText(status)));
    if (SteamGridDbAccount::signingIn(status.state)) {
        page.rows.push_back(
            ui::makeRow(cancelId, RowKind::Action, "Cancel", "Ends the sign-in through Steam", ""));
        return page;
    }
    if (status.state == State::Connected) {
        page.rows.push_back(ui::makeRow(disconnectId, RowKind::Action, "Disconnect SteamGridDB",
                                        "Forgets the key", ""));
        return page;
    }
    if (status.state == State::Disconnected) {
        page.rows.push_back(ui::makeRow(connectId, RowKind::Action, "Connect SteamGridDB",
                                        "Signs in through Steam; you confirm Steam's page", ""));
        page.rows.push_back(ui::makeRow(enterId, RowKind::Action, "Enter a key",
                                        "Opens SteamGridDB's API page in your browser", ""));
    }
    return page;
}

bool ArtworkSettings::owns(const std::string& id) {
    return id.starts_with(statusId);
}

void ArtworkSettings::run(const std::string& id, int step) {
    if (step != 0) {
        return;
    }
    if (id == connectId) {
        account_.connect();
    } else if (id == cancelId) {
        account_.cancel();
    } else if (id == enterId) {
        account_.openKeyPage();
        openEntry();
    } else if (id == disconnectId) {
        account_.disconnect();
    }
    hooks_.refresh();
}

void ArtworkSettings::service() {
    for (const SteamGridDbAccount::News& news : account_.take()) {
        hooks_.say(news.text, news.error);
        if (news.keyChanged) {
            hooks_.keyChanged();
        }
        if (news.needsKey) {
            openEntry();
        }
    }
    if (const SteamGridDbAccount::Status status = account_.status(); status != shown_) {
        shown_ = status;
        hooks_.refresh();
    }
}

bool ArtworkSettings::typing() const noexcept {
    return entering_ && entry_.isOpen();
}

void ArtworkSettings::openEntry() {
    sounds_.play(audio::Effect::Open);
    entry_.configure("SteamGridDB API key", ui::EntryKind::Password);
    entry_.open({});
    entering_ = true;
}

void ArtworkSettings::act(gamepad::Button button) {
    switch (actOnKeyboard(entry_, button, sounds_)) {
    case KeyboardOutcome::Confirm:
        confirm();
        break;
    case KeyboardOutcome::Leave:
        dismiss();
        break;
    case KeyboardOutcome::Edited:
    case KeyboardOutcome::Handled:
        break;
    }
}

void ArtworkSettings::typeText(std::string_view text) {
    if (typing()) {
        static_cast<void>(entry_.type(text));
    }
}

void ArtworkSettings::backspace() {
    if (typing()) {
        static_cast<void>(entry_.backspace());
    }
}

void ArtworkSettings::confirm() {
    if (!typing()) {
        return;
    }
    std::string key = entry_.takeText();
    entering_ = false;
    entry_.close();
    sounds_.play(audio::Effect::Close);
    account_.useKey(std::move(key));
}

void ArtworkSettings::dismiss() {
    if (!typing()) {
        return;
    }
    entering_ = false;
    entry_.close();
    sounds_.play(audio::Effect::Close);
}

} // namespace opensu::app
