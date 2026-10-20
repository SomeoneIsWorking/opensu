// switch_package — the files inside a Switch game file: an NSP's PFS0, or the secure partition of
// an XCI's HFS0 tree (a card image with or without its 0x1000-byte key area in front); reads at a
// position; the title key a common ticket in the package holds.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "switch_keys.hpp"

namespace opensu::library::roms {

/// The little-endian integer at `at` in `bytes`; the caller has checked it fits.
template <class T>
    requires std::is_unsigned_v<T>
[[nodiscard]] T littleEndian(std::span<const std::uint8_t> bytes, std::size_t at) {
    T value = 0;
    for (std::size_t i = sizeof(T); i > 0; --i) {
        value = static_cast<T>((value << 8U) | bytes[at + i - 1]);
    }
    return value;
}

/// One file of the package, its offset from the start of the game file.
struct PackageFile {
    std::string name;
    std::uint64_t offset{0};
    std::uint64_t size{0};
};

class SwitchPackage {
  public:
    /// The package in `file`: the NSP's files, or the XCI's secure partition. Nothing, with
    /// `error` naming the step, when it is neither.
    [[nodiscard]] static std::optional<SwitchPackage> open(const std::filesystem::path& file,
                                                           std::string& error);

    [[nodiscard]] const std::vector<PackageFile>& files() const noexcept {
        return files_;
    }

    /// Fills `out` from `offset` in the game file. False past its end or on a read error.
    [[nodiscard]] bool read(std::uint64_t offset, std::span<std::uint8_t> out);

    /// The encrypted title key of the common ticket for `rights` in the package (`<rights>.tik`).
    [[nodiscard]] std::optional<Key128> ticketTitleKey(const RightsId& rights);

  private:
    SwitchPackage(std::ifstream in, std::uint64_t size);

    /// The files of the PFS0 or HFS0 at `at`, added to `out`.
    [[nodiscard]] bool readPartition(std::uint64_t at, bool hashed, std::vector<PackageFile>& out,
                                     std::string& error);
    [[nodiscard]] bool openCard(std::string& error);

    std::ifstream in_;
    std::uint64_t size_{0};
    std::vector<PackageFile> files_;
};

} // namespace opensu::library::roms
