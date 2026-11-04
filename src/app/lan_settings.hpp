// lan_settings — the Settings screen's LAN category while LAN is on: the name this openSU goes by
// (renamed on an entry card), whether it can be found and on which port, the other openSUs found
// on the LAN with Pair, and the paired ones with Remove, which asks first, and Share saves, off
// until the player turns it on.
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "lan_service.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class LanSettings {
  public:
    /// After the name changed: the screen shows it.
    using Changed = std::function<void()>;

    LanSettings(LanService& lan, Preferences& preferences, gpu2d::ui::Navigator& navigator,
                KeyboardPad& keyboard, audio::SdlSoundPlayer& sounds, Changed changed)
        : lan_{lan}, preferences_{preferences}, navigator_{navigator}, keyboard_{keyboard},
          sounds_{sounds}, changed_{std::move(changed)} {
    }

    /// The category's id, which its rows' ids start with.
    static constexpr std::string_view categoryId = "lan";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: the name opens its card, Pair pairs, Remove asks first, Share saves
    /// turns over.
    void run(const std::string& id, int step);

  private:
    void askName();
    void closeName();

    LanService& lan_;
    Preferences& preferences_;
    gpu2d::ui::Navigator& navigator_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Changed changed_;
    /// The name as the card edits it.
    std::string draft_;
};

} // namespace opensu::app
