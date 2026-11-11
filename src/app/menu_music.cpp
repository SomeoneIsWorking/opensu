#include "menu_music.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "audio/sound_clip.hpp"
#include "lucent/log.h"

namespace opensu::app {

void MenuMusic::service(const std::shared_ptr<const theme::InterfaceTheme>& theme, bool wanted) {
    if (wanted && theme != theme_) {
        theme_ = theme;
        loop_.reset();
        if (!theme->music.empty()) {
            std::optional<audio::SoundClip> clip = audio::decodeSound(theme->music);
            if (clip) {
                const double seconds =
                    static_cast<double>(clip->frames()) / std::max(clip->spec.freq, 1);
                loop_ = sounds_.loop(std::move(*clip));
                lucent::info("audio", "menu music {}: {:.1f} s from {}{}", theme->name, seconds,
                             theme->music.string(),
                             loop_ == nullptr ? ", no device to play it" : "");
            }
        }
    }
    const bool play = wanted && loop_ != nullptr;
    if (loop_ != nullptr && play != loop_->playing()) {
        loop_->play(play);
        lucent::info("audio", "menu music {}", play ? "plays" : "stops");
    }
}

} // namespace opensu::app
