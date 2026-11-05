// profile_settings — Settings › Profiles: each profile with its name (renamed on an entry card),
// its picture and Play as, and Add profile, which names a new one on the same card.
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "profiles/profile_book.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class ProfileSettings {
  public:
    struct Hooks {
        /// A profile changed: the screen shows it.
        std::function<void()> changed;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// Makes the profile `id` the one games play as (`ShellApp::switchProfile`).
        std::function<void(const std::string& id)> play;
    };

    ProfileSettings(profiles::ProfileBook& profiles, gpu2d::ui::Navigator& navigator,
                    KeyboardPad& keyboard, audio::SdlSoundPlayer& sounds, Hooks hooks)
        : profiles_{profiles}, navigator_{navigator}, keyboard_{keyboard}, sounds_{sounds},
          hooks_{std::move(hooks)} {
    }

    static constexpr std::string_view categoryId = "profiles";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: a name opens its card, a picture moves `step` colours, Play as makes the
    /// profile the active one, Add profile names a new one.
    void run(const std::string& id, int step);

  private:
    /// The entry card for `profileId`'s name, or a new profile's when it is empty.
    void askName(std::string profileId);
    void closeName();
    void submitName();

    profiles::ProfileBook& profiles_;
    gpu2d::ui::Navigator& navigator_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    /// The profile the card names, empty for a new one, and the name as the card edits it.
    std::string naming_;
    std::string draft_;
};

} // namespace opensu::app
