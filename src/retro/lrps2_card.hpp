// lrps2_card — LRPS2's memory card in the profile's saves. The catalog turns its shared cards off
// (`pcsx2_shared_memory_cards=disabled`), so LRPS2 keeps one card per game in the runner's saves
// folder, `lrps2CardFile`. A game without one there starts from a copy of the card LRPS2 shared
// between every game before, `<system>/pcsx2/memcards/Mcd001.ps2`, which is left as it was.
#pragma once

#include <filesystem>
#include <string_view>

namespace opensu::retro {

/// The core name (`coreName`) of LRPS2.
inline constexpr std::string_view lrps2Core = "pcsx2";

/// LRPS2's shared card in slot 1, under the system folder.
[[nodiscard]] std::filesystem::path lrps2SharedCard(const std::filesystem::path& system);

/// Copies the shared card as `rom`'s card in `saves` when the game has none and the shared card
/// exists; never writes over a card. Whether it copied.
bool adoptLrps2SharedCard(const std::filesystem::path& system, const std::filesystem::path& saves,
                          const std::filesystem::path& rom);

} // namespace opensu::retro
