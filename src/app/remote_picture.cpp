#include "remote_picture.hpp"

#include <span>
#include <string>
#include <utility>

#include "gpu2d/gpu_error.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "lucent/log.h"
#include "stream/video_codec.hpp"

namespace opensu::app {
namespace {

FramePng encoded(std::span<const std::uint8_t> rgba, int width, int height, std::uint32_t number) {
    std::string error;
    std::string png = gpu2d::image::encodePng(rgba, static_cast<unsigned>(width),
                                              static_cast<unsigned>(height), error);
    if (png.empty()) {
        lucent::error("remote", "could not encode frame {}: {}", number, error);
    }
    return FramePng{.png = std::move(png), .number = number};
}

} // namespace

RemotePicture::RemotePicture(SDL_GPUDevice& device, bool importing) : device_{&device} {
    if (importing) {
        imported_.emplace(device);
    }
}

bool RemotePicture::show(const stream::GuestVideo::Frame& frame) {
    const stream::Picture& picture = frame.picture;
    if (picture.dmabuf) {
        if (!imported_) {
            lucent::warn("remote", "frame {} lies in DMA-BUFs this device cannot sample",
                         frame.number);
            return false;
        }
        try {
            imported_->show(*picture.dmabuf, stream::rgbFromBt709());
        } catch (const gpu2d::GpuError& failure) {
            lucent::warn("remote", "frame {} cannot be sampled where it was decoded: {}",
                         frame.number, failure.what());
            return false;
        }
        texture_.reset();
        showsImported_ = true;
    } else {
        texture_.emplace(*device_, picture.rgba, picture.width, picture.height,
                         gpu2d::Filter::Linear);
        showsImported_ = false;
    }
    width_ = picture.width;
    height_ = picture.height;
    number_ = frame.number;
    fresh_ = true;
    return true;
}

void RemotePicture::clear() {
    texture_.reset();
    showsImported_ = false;
    fresh_ = false;
}

std::optional<gpu2d::TextureView> RemotePicture::view() const {
    if (texture_) {
        return texture_->view();
    }
    if (imported_ && showsImported_) {
        return imported_->view();
    }
    return std::nullopt;
}

display::Placed RemotePicture::placement(display::Size window) const {
    if (!texture_ && !showsImported_) {
        return display::Placed{.x = 0, .y = 0, .width = window.width, .height = window.height};
    }
    return display::letterbox(window, static_cast<float>(width_) / static_cast<float>(height_));
}

std::optional<std::uint32_t> RemotePicture::takeFresh() {
    if (!std::exchange(fresh_, false)) {
        return std::nullopt;
    }
    return number_;
}

FramePng RemotePicture::png(const std::optional<stream::GuestVideo::Frame>& newest) const {
    if (!newest) {
        return {};
    }
    if (!newest->picture.dmabuf) {
        return encoded(newest->picture.rgba, newest->picture.width, newest->picture.height,
                       newest->number);
    }
    if (!imported_ || !showsImported_) {
        return {};
    }
    const gpu2d::image::Bitmap shown = imported_->read();
    return encoded(shown.rgba, shown.width, shown.height, number_);
}

} // namespace opensu::app
