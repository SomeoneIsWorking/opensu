#include "save_ram_fit.hpp"

#include <algorithm>

namespace opensu::retro {
namespace {

/// mGBA's packed MBC2 RAM and gambatte's bank that holds it, one cell a byte.
constexpr std::size_t packedMbc2 = 0x100;
constexpr std::size_t mbc2Cells = 0x200;
constexpr std::size_t gambatteBank = 0x2000;
constexpr unsigned nibble = 0x0F;
constexpr unsigned unwiredBits = 0xF0;
constexpr unsigned nibbleBits = 4;
/// `GBASavedataRTCBuffer`, which no libretro save RAM holds.
constexpr std::size_t mgbaClockBytes = 16;

bool blank(std::span<const std::byte> bytes) {
    return std::ranges::all_of(bytes,
                               [](std::byte b) {
                                   return b == std::byte{0};
                               }) ||
           std::ranges::all_of(bytes, [](std::byte b) {
               return b == std::byte{0xFF};
           });
}

} // namespace

std::optional<std::vector<std::byte>> fittedSaveRam(std::span<const std::byte> file,
                                                    std::size_t ramBytes) {
    if (file.size() == gambatteBank && ramBytes == packedMbc2) {
        std::vector<std::byte> packed(packedMbc2);
        for (std::size_t i = 0; i < packedMbc2; ++i) {
            const auto low = std::to_integer<unsigned>(file[2 * i]) & nibble;
            const auto high = std::to_integer<unsigned>(file[(2 * i) + 1]) & nibble;
            packed[i] = static_cast<std::byte>(low | (high << nibbleBits));
        }
        return packed;
    }
    if (file.size() == packedMbc2 && ramBytes == gambatteBank) {
        // The cells repeat through the bank as the cartridge mirrors them; the unwired bits read 1.
        std::vector<std::byte> cells(gambatteBank);
        for (std::size_t i = 0; i < gambatteBank; ++i) {
            const std::size_t cell = i % mbc2Cells;
            const auto packed = std::to_integer<unsigned>(file[cell / 2]);
            const unsigned value = (cell % 2 == 0 ? packed : packed >> nibbleBits) & nibble;
            cells[i] = static_cast<std::byte>(value | unwiredBits);
        }
        return cells;
    }
    if (file.size() > ramBytes && ramBytes > 0 &&
        (file.size() == ramBytes + mgbaClockBytes || blank(file.subspan(ramBytes)))) {
        return std::vector<std::byte>{file.begin(), file.begin() + static_cast<long>(ramBytes)};
    }
    return std::nullopt;
}

} // namespace opensu::retro
