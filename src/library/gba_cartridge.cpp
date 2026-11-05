#include "gba_cartridge.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace opensu::library {
namespace {

constexpr std::size_t gameCodeAt = 0xAC;
constexpr std::size_t gameCodeBytes = 4;
// Nintendo's RTC library leaves this in every game that links it, ROM hacks included.
constexpr std::string_view clockLibrary = "SIIRTC_V";
// `HW_RTC` entries of mGBA's src/gba/overrides.c.
constexpr std::array<std::string_view, 33> clockGames{
    "U3IJ", "U3IE", "U3IP", "U32J", "U32E", "U32P", "BLJJ", "BLJK", "BLVJ", "AXVJ", "AXVE",
    "AXVP", "AXVI", "AXVS", "AXVD", "AXVF", "AXPJ", "AXPE", "AXPP", "AXPI", "AXPS", "AXPD",
    "AXPF", "BPEJ", "BPEE", "BPEP", "BPEI", "BPES", "BPED", "BPEF", "BR4J", "BKAJ", "U33J",
};

} // namespace

bool hasRealTimeClock(std::string_view rom) {
    if (rom.size() >= gameCodeAt + gameCodeBytes &&
        std::ranges::find(clockGames, rom.substr(gameCodeAt, gameCodeBytes)) != clockGames.end()) {
        return true;
    }
    return rom.find(clockLibrary) != std::string_view::npos;
}

} // namespace opensu::library
