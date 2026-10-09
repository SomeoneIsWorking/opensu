// libretro_index — finding a ROM's box art in libretro-thumbnails, whose files are named by
// No-Intro and Redump titles ("Legend of Zelda, The - The Wind Waker (USA).png").
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::artwork {

/// The thumbnail names in a Named_Boxarts directory listing, decoded and without `.png`.
[[nodiscard]] std::vector<std::string> parseIndex(std::string_view html);

/// A name as libretro writes it in a file name: `&*/:`<>?\|"` become `_`.
[[nodiscard]] std::string thumbnailName(std::string_view name);

/// The index entry for a ROM named `romName`: the same name if listed, else the same title
/// in the ROM's own region, else USA, World, Europe, Japan, then any, never a beta, demo or
/// prototype the ROM is not. Nothing when no entry has the same title.
[[nodiscard]] std::optional<std::string> bestMatch(std::string_view romName,
                                                   std::span<const std::string> index);

/// Percent-encodes a path segment, leaving parentheses and commas as libretro's links do.
[[nodiscard]] std::string encodeSegment(std::string_view segment);

} // namespace opensu::artwork
