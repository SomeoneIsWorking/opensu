// artwork_settings — Settings › Artwork: SteamGridDB's connection (`SteamGridDbAccount`) with its
// status, Connect (through Steam's browser, else the key typed in), Cancel, Enter a key and
// Disconnect, and the on-screen keyboard the key is typed or pasted into. The key shows as dots and
// is wiped when the keyboard closes.
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "audio/sdl_sound_player.hpp"
#include "gamepad/event.hpp"
#include "steamgriddb_account.hpp"
#include "ui/on_screen_keyboard.hpp"
#include "ui/settings_page.hpp"

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

    ArtworkSettings(SteamGridDbAccount& account, ui::OnScreenKeyboard& entry,
                    audio::SdlSoundPlayer& sounds, Hooks hooks)
        : account_{account}, entry_{entry}, sounds_{sounds}, hooks_{std::move(hooks)} {
    }

    static constexpr std::string_view categoryId = "artwork";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id` (A only).
    void run(const std::string& id, int step);
    /// Takes what the account did. Main loop only.
    void service();

    /// Whether the key keyboard is up, which takes every button and a physical keyboard's text.
    [[nodiscard]] bool typing() const noexcept;
    void act(gamepad::Button button);
    void typeText(std::string_view text);
    void backspace();
    /// Enter or Done: keeps the typed key.
    void confirm();
    /// Escape: closes the keyboard, the key unkept.
    void dismiss();

  private:
    void openEntry();

    SteamGridDbAccount& account_;
    ui::OnScreenKeyboard& entry_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    /// Whether the keyboard is up for the key, not for another entry sharing it.
    bool entering_{false};
    SteamGridDbAccount::Status shown_{};
};

} // namespace opensu::app
