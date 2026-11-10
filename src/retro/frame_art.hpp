// frame_art — the art a visual preset frames the picture with (`VideoSettings::landscapeArt` and
// `portraitArt`), decoded from its PNG once per change of settings, which of the two a room of
// the target takes by its shape, and where the art and the picture stand in that room: the art
// letterboxed into it, the picture inside the art's picture rect. The frame compositor draws it.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include "settings/video_settings.hpp"

namespace opensu::retro {

/// Decoded art, RGBA rows top first, straight alpha.
struct ArtImage {
    settings::FrameArt art;
    unsigned width{0};
    unsigned height{0};
    std::vector<std::uint8_t> rgba;
};

/// Where framed art and its picture stand.
struct Framing {
    VkRect2D art{};
    /// The art's picture rect on the target: the room the picture is placed in.
    VkRect2D picture{};
};

/// `image` letterboxed into `room`, and its picture rect scaled with it.
[[nodiscard]] Framing frameIn(VkRect2D room, const ArtImage& image);

class FrameArtImages {
  public:
    /// Decodes the art `video` names that differs from what is held; art that cannot be read or
    /// whose picture rect leaves the image is dropped, said in the log.
    void load(const settings::VideoSettings& video);

    /// The art a room of `extent` takes: the portrait art when it is taller than wide, else the
    /// landscape; null when that one is none.
    [[nodiscard]] const ArtImage* forRoom(VkExtent2D extent) const;

  private:
    std::optional<ArtImage> landscape_;
    std::optional<ArtImage> portrait_;
};

} // namespace opensu::retro
