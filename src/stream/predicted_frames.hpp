// predicted_frames — a hardware codec's frames as their packets come, for its decoder: the parts
// put together in order, and the rule that a frame lost leaves the ones after it nothing to build
// on, so nothing is decoded until the next key frame.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "stream_wire.hpp"

namespace opensu::stream {

class PredictedFrames {
  public:
    /// Takes one packet; true once its frame has all of its parts and something to build on.
    bool push(const wire::VideoPacket& packet);
    /// The whole frame `push` said can be decoded.
    [[nodiscard]] std::string coded() const;
    /// The decoder failed: nothing more is decoded until a key frame.
    void lost() noexcept {
        awaitingKey_ = true;
    }
    [[nodiscard]] bool awaitingKey() const noexcept {
        return awaitingKey_;
    }

  private:
    /// The frame whose packets are coming, by part.
    std::uint32_t frame_{0};
    std::size_t expected_{0};
    bool key_{false};
    bool whole_{false};
    std::map<std::uint16_t, std::string> parts_;
    bool awaitingKey_{true};
};

} // namespace opensu::stream
