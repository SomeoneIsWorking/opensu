#include "predicted_frames.hpp"

#include "frame_parity.hpp"

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
        lastBytes_ = packet.lastBytes;
        key_ = packet.key;
        whole_ = false;
        parts_.clear();
        parity_.clear();
    }
    if (whole_) {
        return false;
    }
    if (packet.part < expected_) {
        parts_[packet.part] = packet.data;
    } else {
        parity_[static_cast<std::uint16_t>(packet.part - expected_)] = packet.data;
    }
    if (parts_.size() < expected_ && parts_.size() + parity_.size() >= expected_) {
        restoreParts(parts_, parity_, expected_, lastBytes_);
    }
    if (parts_.size() != expected_) {
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
