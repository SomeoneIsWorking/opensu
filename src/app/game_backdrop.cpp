#include "game_backdrop.hpp"

#include <span>
#include <string>

#include "gpu2d/image/decode.hpp"
#include "lucent/log.h"

namespace opensu::app {

void GameBackdrop::follow(bool wanted, RetroSession& game) {
    if (wanted == wanted_) {
        return;
    }
    wanted_ = wanted;
    texture_.reset();
    if (!wanted) {
        return;
    }
    const std::optional<std::string> png = game.frame();
    if (!png) {
        lucent::info("shell", "no frame of the game to show behind the menu");
        return;
    }
    std::string error;
    const std::optional<gpu2d::image::Bitmap> frame =
        gpu2d::image::decode(std::as_bytes(std::span{*png}), error);
    if (!frame) {
        lucent::warn("shell", "the game's frame cannot be read: {}", error);
        return;
    }
    texture_.emplace(*device_, frame->rgba, frame->width, frame->height, gpu2d::Filter::Linear);
    lucent::info("shell", "the game's last frame {}x{} behind the menu", frame->width,
                 frame->height);
}

std::optional<gpu2d::TextureView> GameBackdrop::view() const {
    if (texture_) {
        return texture_->view();
    }
    return std::nullopt;
}

} // namespace opensu::app
