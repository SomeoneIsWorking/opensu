// artwork_settings — Settings › Artwork: the console art theme the home shows (`ConsoleThemes`:
// the shipped ones, the player's own, or iiSU's from the release APK or starter pack the player
// downloaded, picked on `IisuFileChooser`), SteamGridDB's connection
// (`SteamGridDbAccount`) with its status, Connect (through Steam's browser, else the key typed in),
// Cancel, Enter a key and Disconnect, and the card the key is typed or pasted into, with the
// on-screen keyboard up. The key shows as dots and is wiped when the card closes.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "artwork/console_themes.hpp"
#include "audio/sdl_sound_player.hpp"
#include "iisu_file_chooser.hpp"
#include "keyboard_pad.hpp"
#include "preferences.hpp"
#include "steamgriddb_account.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class ArtworkSettings {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Artwork is asked for again: the key came or went.
        std::function<void()> keyChanged;
        /// The rows changed.
        std::function<void()> refresh;
        /// The player chose another console art theme.
        std::function<void()> consoleArtChanged;
    };

    /// What the key card goes on and comes up with.
    struct Entry {
        gpu2d::ui::Navigator& navigator;
        KeyboardPad& keyboard;
    };

    /// What the page reads and changes besides the account.
    struct Services {
        Preferences& preferences;
        const artwork::ConsoleThemes& themes;
        /// The chooser iiSU's file is picked on.
        IisuFileChooser& chooser;
        audio::SdlSoundPlayer& sounds;
    };

    ArtworkSettings(SteamGridDbAccount& account, Entry entry, Services services, Hooks hooks)
        : account_{account}, navigator_{entry.navigator}, keyboard_{entry.keyboard},
          preferences_{services.preferences}, themes_{services.themes}, chooser_{services.chooser},
          sounds_{services.sounds}, hooks_{std::move(hooks)} {
    }
    ~ArtworkSettings();
    ArtworkSettings(const ArtworkSettings&) = delete;
    ArtworkSettings& operator=(const ArtworkSettings&) = delete;
    ArtworkSettings(ArtworkSettings&&) = delete;
    ArtworkSettings& operator=(ArtworkSettings&&) = delete;

    static constexpr std::string_view categoryId = "artwork";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: the console art theme steps either way, the rest take A only.
    void run(const std::string& id, int step);
    /// Starts connecting SteamGridDB through Steam, as the Connect row does.
    void connect();
    /// Takes what the account did. Main loop only.
    void service();

    /// Whether the key card is up.
    [[nodiscard]] bool typing() const;

  private:
    /// Opens the file chooser for iiSU's release or starter pack; a file taken chooses iiSU's
    /// theme.
    void chooseIisuFile();
    /// Keeps `theme` and `iisuFile` and has the home show them.
    void useTheme(std::string theme, const std::filesystem::path& iisuFile);
    void openEntry();
    /// Submit: keeps the typed key.
    void confirm();
    /// Closes the card; the key is wiped.
    void dismiss();
    [[nodiscard]] const gpu2d::ui::Screen* keyCard() const;

    SteamGridDbAccount& account_;
    gpu2d::ui::Navigator& navigator_;
    KeyboardPad& keyboard_;
    Preferences& preferences_;
    const artwork::ConsoleThemes& themes_;
    IisuFileChooser& chooser_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    /// The key as typed.
    std::string key_;
    SteamGridDbAccount::Status shown_{};
};

} // namespace opensu::app
