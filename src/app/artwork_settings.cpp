#include "artwork_settings.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "host/secret.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;
using State = SteamGridDbAccount::State;

constexpr const char* consoleArtId = "artwork.consoleart";
constexpr const char* statusId = "artwork.steamgriddb";
constexpr const char* connectId = "artwork.steamgriddb.connect";
constexpr const char* cancelId = "artwork.steamgriddb.cancel";
constexpr const char* enterId = "artwork.steamgriddb.enter";
constexpr const char* disconnectId = "artwork.steamgriddb.disconnect";
constexpr std::string_view keyCardId = "artwork.key";
/// The most bytes a key holds.
constexpr std::size_t keyMostLength = 256;

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
    const std::vector<artwork::ThemeInfo> themes = themes_.list();
    const std::string& chosen = preferences_.values().consoleTheme;
    ui::Options choices;
    std::string themeNote;
    for (std::size_t i = 0; i < themes.size(); ++i) {
        choices.labels.push_back(themes[i].name);
        const bool current = themes[i].id == (chosen.empty() ? artwork::defaultConsoleTheme
                                                             : std::string_view{chosen});
        if (current) {
            choices.chosen = i;
            themeNote = themes[i].note;
        }
    }
    if (chosen == artwork::iisuConsoleTheme) {
        themeNote = "iiSU's, from " + preferences_.values().iisuFile.filename().string();
    }
    page.rows.push_back(
        ui::makeChoice(consoleArtId, "Console art theme", themeNote, std::move(choices)));
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
        ui::SettingsRow disconnect = ui::makeRow(disconnectId, RowKind::Action,
                                                 "Disconnect SteamGridDB", "Forgets the key", "");
        disconnect.confirm = "openSU forgets the key; Connect signs in again.";
        page.rows.push_back(std::move(disconnect));
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
    return id == consoleArtId || id.starts_with(statusId);
}

void ArtworkSettings::run(const std::string& id, int step) {
    if (id == consoleArtId) {
        const std::vector<artwork::ThemeInfo> themes = themes_.list();
        const std::string& chosen = preferences_.values().consoleTheme;
        const auto found = std::ranges::find_if(themes, [&chosen](const artwork::ThemeInfo& theme) {
            return theme.id ==
                   (chosen.empty() ? artwork::defaultConsoleTheme : std::string_view{chosen});
        });
        const auto from = static_cast<int>(found == themes.end() ? 0 : found - themes.begin());
        const auto count = static_cast<int>(themes.size());
        const int at = ((from + (step == 0 ? 1 : step)) % count + count) % count;
        const std::string& next = themes[static_cast<std::size_t>(at)].id;
        // iiSU's asks for its file first.
        if (next == artwork::iisuConsoleTheme) {
            chooseIisuFile();
            return;
        }
        useTheme(next == artwork::defaultConsoleTheme ? std::string{} : next,
                 preferences_.values().iisuFile);
        return;
    }
    if (step != 0) {
        return;
    }
    if (id == connectId) {
        connect();
        return;
    }
    if (id == cancelId) {
        account_.cancel();
    } else if (id == enterId) {
        account_.openKeyPage();
        openEntry();
    } else if (id == disconnectId) {
        account_.disconnect();
    }
    hooks_.refresh();
}

void ArtworkSettings::useTheme(std::string theme, const std::filesystem::path& iisuFile) {
    preferences_.values().consoleTheme = std::move(theme);
    preferences_.values().iisuFile = iisuFile;
    preferences_.save();
    hooks_.consoleArtChanged();
    hooks_.refresh();
}

void ArtworkSettings::chooseIisuFile() {
    chooser_.choose(IisuFileChooser::Need::Art, preferences_.values().iisuFile,
                    [this](const std::filesystem::path& file) {
                        useTheme(std::string{artwork::iisuConsoleTheme}, file);
                    });
}

void ArtworkSettings::connect() {
    account_.connect();
    // The page shows the state connect set; a sign-in that fails before the next service must
    // still differ from what is shown.
    shown_ = account_.status();
    hooks_.refresh();
}

void ArtworkSettings::service() {
    const SteamGridDbAccount::Taken taken = account_.take();
    for (const SteamGridDbAccount::News& news : taken.news) {
        hooks_.say(news.text, news.error);
        if (news.keyChanged) {
            hooks_.keyChanged();
        }
        if (news.needsKey) {
            openEntry();
        }
    }
    const SteamGridDbAccount::Status& status = taken.status;
    if (status != shown_) {
        const bool connected = status.state == State::Connected && shown_.state != State::Connected;
        const std::string focused = navigator_.focused();
        shown_ = status;
        hooks_.refresh();
        // The row pressed is gone, and the nearest one in its place is Disconnect.
        if (connected && (focused == connectId || focused == cancelId || focused == enterId)) {
            navigator_.focus(consoleArtId);
        }
    }
}

ArtworkSettings::~ArtworkSettings() {
    host::wipe(key_);
}

const gpu2d::ui::Screen* ArtworkSettings::keyCard() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return screen->id() == keyCardId && !screen->closing();
    });
    return found == open.end() ? nullptr : *found;
}

bool ArtworkSettings::typing() const {
    return keyCard() != nullptr;
}

void ArtworkSettings::openEntry() {
    if (typing()) {
        return;
    }
    sounds_.play(audio::Effect::Open);
    host::wipe(key_);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = std::string{keyCardId},
                  .title = "SteamGridDB API key",
                  .note = "Paste or type the key from SteamGridDB's API page.",
                  .label = "API key",
                  .placeholder = "Key",
                  .secret = true,
                  .mostBytes = keyMostLength,
                  .text = [this]() -> const std::string& {
                      return key_;
                  },
                  .changed =
                      [this](std::string text) {
                          host::wipe(key_);
                          key_ = std::move(text);
                      },
                  .submitted =
                      [this] {
                          confirm();
                      },
                  .cancelled =
                      [this] {
                          dismiss();
                      }}));
    keyboard_.open();
}

void ArtworkSettings::confirm() {
    if (const gpu2d::ui::Screen* card = keyCard()) {
        navigator_.closeFrom(*card);
    }
    sounds_.play(audio::Effect::Close);
    account_.useKey(std::exchange(key_, std::string{}));
}

void ArtworkSettings::dismiss() {
    if (const gpu2d::ui::Screen* card = keyCard()) {
        navigator_.closeFrom(*card);
    }
    sounds_.play(audio::Effect::Close);
    host::wipe(key_);
}

} // namespace opensu::app
