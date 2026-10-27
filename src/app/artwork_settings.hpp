// artwork_settings — Settings › Artwork: SteamGridDB's connection (`SteamGridDbAccount`) with its
// status, Connect (through Steam's browser, else the key typed in), Cancel, Enter a key and
// Disconnect, and the card the key is typed or pasted into, with the on-screen keyboard up. The key
// shows as dots and is wiped when the card closes.
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
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
    };

    /// What the key card goes on and comes up with.
    struct Entry {
        gpu2d::ui::Navigator& navigator;
        KeyboardPad& keyboard;
    };

    ArtworkSettings(SteamGridDbAccount& account, Entry entry, audio::SdlSoundPlayer& sounds,
                    Hooks hooks)
        : account_{account}, navigator_{entry.navigator}, keyboard_{entry.keyboard},
          sounds_{sounds}, hooks_{std::move(hooks)} {
    }
    ~ArtworkSettings();
    ArtworkSettings(const ArtworkSettings&) = delete;
    ArtworkSettings& operator=(const ArtworkSettings&) = delete;
    ArtworkSettings(ArtworkSettings&&) = delete;
    ArtworkSettings& operator=(ArtworkSettings&&) = delete;

    static constexpr std::string_view categoryId = "artwork";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id` (A only).
    void run(const std::string& id, int step);
    /// Starts connecting SteamGridDB through Steam, as the Connect row does.
    void connect();
    /// Takes what the account did. Main loop only.
    void service();

    /// Whether the key card is up.
    [[nodiscard]] bool typing() const;

  private:
    void openEntry();
    /// Submit: keeps the typed key.
    void confirm();
    /// Closes the card; the key is wiped.
    void dismiss();
    [[nodiscard]] const gpu2d::ui::Screen* keyCard() const;

    SteamGridDbAccount& account_;
    gpu2d::ui::Navigator& navigator_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    /// The key as typed.
    std::string key_;
    SteamGridDbAccount::Status shown_{};
};

} // namespace opensu::app
