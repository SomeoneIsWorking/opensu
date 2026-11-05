// libretro_index — finding a ROM's box art in libretro-thumbnails, whose files are named by
// No-Intro and Redump titles ("Legend of Zelda, The - The Wind Waker (USA).png").
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "library/rom_systems.hpp"
#include "net/web_client.hpp"

namespace opensu::artwork {

class ArtworkStore;

/// The systems a PC game's title may have box art on, likeliest first: those PC games were ported
/// to and from.
inline constexpr std::array<std::string_view, 9> boxArtSystems{
    "psx", "ps2", "dreamcast", "gc", "xbox", "ps3", "xbox360", "psp", "wii"};

/// The thumbnail names in a Named_Boxarts directory listing, decoded and without `.png`.
[[nodiscard]] std::vector<std::string> parseIndex(std::string_view html);

/// A name as libretro writes it in a file name: `&*/:`<>?\|"` become `_`.
[[nodiscard]] std::string thumbnailName(std::string_view name);

/// The index entry for a ROM named `romName`: the same name if listed, else the same title
/// in the ROM's own region, else USA, World, Europe, Japan, then any, never a beta, demo or
/// prototype the ROM is not. Nothing when no entry has the same title.
[[nodiscard]] std::optional<std::string> bestMatch(std::string_view romName,
                                                   std::span<const std::string> index);

/// The box art of a game that is no ROM, known only by `title`, which it may share with another
/// game: the index entry `bestMatch` ranks first among those of exactly that title, and only when
/// the title has two words or more besides "the" and "and" and no bracket of its own. A demo, beta,
/// prototype, hack, pirate, unlicensed, aftermarket or homebrew release never counts. A wrong cover
/// is worse than the game's own icon, so anything less is nothing.
[[nodiscard]] std::optional<std::string> confidentMatch(std::string_view title,
                                                        std::span<const std::string> index);

/// The entries of `index` whose title has every word of `term` (letters and digits, ignoring
/// case, "the" and "and"), in listing order, `most` at most.
[[nodiscard]] std::vector<std::string>
namesMatching(std::string_view term, std::span<const std::string> index, std::size_t most);

/// Percent-encodes a path segment, leaving parentheses and commas as libretro's links do.
[[nodiscard]] std::string encodeSegment(std::string_view segment);

/// The URL of box art `name` of `system` under libretro-thumbnails' root `base`.
[[nodiscard]] std::string boxArtUrl(std::string_view base, const library::roms::RomSystem& system,
                                    std::string_view name);

/// `system`'s box art names: the listing `store` keeps while it is fresh, else the one under
/// `base` downloaded on `stop` and kept there.
[[nodiscard]] net::Result<std::vector<std::string>>
boxArtNames(const std::stop_token& stop, const net::WebClient& web, const ArtworkStore& store,
            const library::roms::RomSystem& system, std::string_view base);

} // namespace opensu::artwork
