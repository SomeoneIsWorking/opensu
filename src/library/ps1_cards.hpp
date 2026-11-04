// ps1_cards — PS1 memory cards as DuckStation keeps them: a card per game in its `memcards`
// folder (`[MemoryCards] Directory` of its `settings.ini` overriding it), named
// `<title>_1.mcd` by the card type `[MemoryCards] Card1Type` (`System::GetGameMemoryCardPath`):
// PerGameTitle, the default, by its database's title, else the file's; PerGameFileTitle by the
// file's. A card is the raw 128 KiB image (`MemoryCardImage::DATA_SIZE`), which DuckStation loads
// only at exactly that size; the libretro cores' `.srm` (pcsx_rearmed, Beetle PSX, SwanStation)
// is the same image of card 1.
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace opensu::library::saves {

/// A PS1 memory card's bytes.
inline constexpr std::size_t ps1CardBytes = std::size_t{128} * 1024;

/// Where DuckStation, its data folder `data` (`~/.local/share/duckstation`) holding its
/// `settings.ini` and `memcards`, keeps the first card of the game `title` (openSU's) in `rom`:
/// the card by the title when it is there or the file's is not, else the file's. Nothing when its
/// card type keeps no card per game (Shared, None) or names it by the disc's serial (PerGame).
[[nodiscard]] std::optional<std::filesystem::path>
duckStationCard(const std::filesystem::path& data, const std::filesystem::path& folder,
                std::string_view title, const std::filesystem::path& rom);

/// `card` checked to be a whole card, as DuckStation and the cores load one. Nothing, with
/// `error`, for any other size.
[[nodiscard]] std::optional<std::string> wholeCard(std::string card, std::string& error);

} // namespace opensu::library::saves
