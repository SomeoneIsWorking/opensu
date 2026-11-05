#include "remote_picture.hpp"

#include <string>
#include <utility>

#include "gpu2d/image/png_encoder.hpp"
#include "lucent/log.h"

namespace opensu::app {

RemotePicture::RemotePicture(SDL_GPUDevice& device) : device_{&device} {}

void RemotePicture::show(const stream::GuestVideo::Frame& frame) {
    const stream::Picture& picture = frame.picture;
    texture_.emplace(*device_, picture.rgba, picture.width, picture.height, gpu2d::Filter::Linear);
    number_ = frame.number;
    fresh_ = true;
}

void RemotePicture::clear() {
    texture_.reset();
    fresh_ = false;
}

std::optional<gpu2d::TextureView> RemotePicture::view() const {
    if (!texture_) {
        return std::nullopt;
    }
    return texture_->view();
}

display::Placed RemotePicture::placement(display::Size window) const {
    if (!texture_) {
        return display::Placed{.x = 0, .y = 0, .width = window.width, .height = window.height};
    }
    return display::letterbox(window, static_cast<float>(texture_->width()) /
                                          static_cast<float>(texture_->height()));
}

std::optional<std::uint32_t> RemotePicture::takeFresh() {
    if (!std::exchange(fresh_, false)) {
        return std::nullopt;
    }
    return number_;
}

FramePng RemotePicture::png(const std::optional<stream::GuestVideo::Frame>& newest) {
    if (!newest) {
        return {};
    }
    std::string error;
    std::string png = gpu2d::image::encodePng(newest->picture.rgba,
                                              static_cast<unsigned>(newest->picture.width),
                                              static_cast<unsigned>(newest->picture.height), error);
    if (png.empty()) {
        lucent::error("remote", "could not encode frame {}: {}", newest->number, error);
    }
    return FramePng{.png = std::move(png), .number = newest->number};
}

} // namespace opensu::app
