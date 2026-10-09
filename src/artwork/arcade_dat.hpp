// arcade_dat — the arcade name listings from libretro-database, downloaded against a pin. The
// listings are FinalBurn Neo's arcade set and MAME 2016's working set; a short name in both reads
// as FinalBurn Neo has it. Parsing and keeping them is `library::roms::NameDb`.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "arcade_names.hpp"
#include "net/web_client.hpp"

namespace opensu::artwork {

/// One listing: its path under `metadat/`, size and CRC-32.
struct DatPin {
    std::string_view path;
    std::uint64_t size;
    std::uint32_t crc32;
};

inline constexpr std::array<DatPin, 2> arcadeDatPins{{
    {"fbneo-split/FBNeo - Arcade Games.dat", 1791303, 0x67cc252a},
    {"mame-split/MAME 2016.dat", 2118571, 0x1ecf3c2e},
}};

/// Where the listings are fetched from at the pinned commit.
inline constexpr std::string_view libretroDatabaseUrl =
    "https://raw.githubusercontent.com/libretro/libretro-database";

/// Downloads every pinned listing under `base`/`revision`/metadat and merges them, earlier ones
/// first. Nothing, with `error`, when a download fails or differs from its pin.
[[nodiscard]] std::optional<library::roms::NameMap> fetchArcadeNames(const net::WebClient& web,
                                                                     std::string_view base,
                                                                     std::span<const DatPin> pins,
                                                                     std::string& error);

} // namespace opensu::artwork
