// switch_nca — one NCA in a Switch package: its header decrypted (AES-XTS with the header key),
// what it holds, and the files of its RomFS section (the IVFC data level, AES-CTR with the key
// from its key area or its title key).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "switch_keys.hpp"
#include "switch_package.hpp"

namespace opensu::library::roms {

/// What an NCA holds (its header's content type).
enum class NcaContent : std::uint8_t {
    Program = 0,
    Meta = 1,
    Control = 2,
    Manual = 3,
    Data = 4,
    PublicData = 5,
};

/// One file of a RomFS, its offset from the start of the RomFS.
struct RomFsFile {
    std::string name;
    std::uint64_t offset{0};
    std::uint64_t size{0};
};

class Nca {
  public:
    /// The NCA stored as `file` in `package`. Nothing, with `error`, when its header does not
    /// decrypt to an NCA3 header.
    [[nodiscard]] static std::optional<Nca> open(SwitchPackage& package, const PackageFile& file,
                                                 const Key256& headerKey, std::string& error);

    [[nodiscard]] NcaContent content() const noexcept {
        return content_;
    }
    /// The title the NCA belongs to.
    [[nodiscard]] std::uint64_t programId() const noexcept {
        return programId_;
    }

    /// The files in the NCA's RomFS section, its key found in `keys` or the package's ticket.
    /// False, with `error` naming the step, when there is none or it cannot be read.
    [[nodiscard]] bool listRomFs(const SwitchKeys& keys, std::vector<RomFsFile>& out,
                                 std::string& error);

    /// A file `listRomFs` named, read and decrypted; nothing above `largest` bytes.
    [[nodiscard]] std::optional<std::string> readRomFs(const RomFsFile& file,
                                                       std::uint64_t largest);

  private:
    /// The RomFS section: where it starts in the NCA, its size, its encryption and counter.
    struct Section {
        std::uint64_t start{0};
        std::uint64_t size{0};
        std::uint8_t encryption{0};
        std::array<std::uint8_t, 8> upperCounter{};
        /// The RomFS (the IVFC data level) within the section.
        std::uint64_t romfsAt{0};
        std::uint64_t romfsSize{0};
    };
    Nca(SwitchPackage& package, const PackageFile& file);

    [[nodiscard]] std::optional<Key128> sectionKey(const SwitchKeys& keys, std::string& error);
    /// `out` from `offset` in `section`, decrypted with `key` (none for a plain section).
    [[nodiscard]] bool readSection(const Section& section, const Key128* key, std::uint64_t offset,
                                   std::span<std::uint8_t> out);

    SwitchPackage* package_;
    PackageFile file_;
    NcaContent content_{NcaContent::Program};
    std::uint64_t programId_{0};
    std::uint8_t keyRevision_{0};
    std::uint8_t keyAreaIndex_{0};
    RightsId rights_{};
    std::array<std::uint8_t, 0x40> keyArea_{};

    std::optional<Section> romfs_;
    std::optional<Key128> key_;
};

} // namespace opensu::library::roms
