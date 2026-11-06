#include "predicted_frames.hpp"

namespace opensu::stream {

bool PredictedFrames::push(const wire::VideoPacket& packet) {
    if (packet.frame < frame_) {
        return false;
    }
    if (packet.frame != frame_) {
        // A frame lost on the way leaves the ones after it nothing to build on.
        if (frame_ != 0 && (!whole_ || packet.frame != frame_ + 1)) {
            awaitingKey_ = true;
        }
        frame_ = packet.frame;
        expected_ = packet.parts;
        key_ = packet.key;
        whole_ = false;
        parts_.clear();
    }
    parts_[packet.part] = packet.data;
    if (whole_ || parts_.size() != expected_) {
        return false;
    }
    whole_ = true;
    if (awaitingKey_ && !key_) {
        return false;
    }
    awaitingKey_ = false;
    return true;
}

std::string PredictedFrames::coded() const {
    std::string all;
    for (const auto& [part, data] : parts_) {
        all += data;
    }
    return all;
}

} // namespace opensu::stream
