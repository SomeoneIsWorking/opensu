// romset_dat — an arcade core's ROM-set listing: every set's ROMs (name, size, CRC-32), its
// parent and BIOS (`romof`), its default BIOS choice, the devices it uses and its disks. Read
// from a ClrMamePro/Logiqx XML datafile (FinalBurn Neo) or a MAME `-listxml` (`game` or
// `machine` elements), and kept under the cache in a line format of its own.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::library::roms {

/// One ROM a set needs. A ROM the listing marks `nodump` or optional, or lists without a CRC,
/// is never kept: nothing could be checked against it.
struct DatRom {
    std::string name;
    std::uint64_t size{0};
    std::uint32_t crc32{0};
    /// The BIOS choice the ROM belongs to; empty when every choice needs it.
    std::string bios;

    bool operator==(const DatRom&) const = default;
};

struct DatSet {
    /// Lower-case short name, the zip's stem.
    std::string name;
    /// The set whose zip lends this one its ROMs: the parent of a clone, else the BIOS.
    std::string romof;
    /// The BIOS choice used unless the player picks another; empty when the set has none.
    std::string defaultBios;
    bool device{false};
    std::vector<DatRom> roms;
    /// The devices whose ROMs the set also needs.
    std::vector<std::string> devices;
    /// The CHD images the set needs, without `.chd`.
    std::vector<std::string> disks;

    bool operator==(const DatSet&) const = default;
};

/// A set's place in the listing, kept for every set even when its ROMs are not.
struct DatLink {
    std::string romof;
    std::string defaultBios;

    bool operator==(const DatLink&) const = default;
};

class RomsetDat {
  public:
    /// Which sets keep their ROMs: those named here and every device that has ROMs or uses
    /// devices. Empty keeps every set. A device with neither is never kept.
    using Wanted = std::set<std::string, std::less<>>;

    /// The sets of a datafile or `-listxml`, merged into this listing; a set already listed stays.
    void addXml(std::string_view xml, const Wanted& wanted = {});

    /// This listing in the cache's line format.
    [[nodiscard]] std::string serialize() const;
    /// The listing `text` holds; nothing, with `error`, when a line is malformed.
    [[nodiscard]] static std::optional<RomsetDat> parse(std::string_view text, const Wanted& wanted,
                                                        std::string& error);

    [[nodiscard]] const DatSet* find(std::string_view name) const;
    [[nodiscard]] const DatLink* link(std::string_view name) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return links_.size();
    }

    bool operator==(const RomsetDat&) const = default;

  private:
    void add(DatSet set, const Wanted& wanted);

    /// In listing order.
    std::vector<DatSet> sets_;
    std::map<std::string, std::size_t, std::less<>> index_;
    std::map<std::string, DatLink, std::less<>> links_;
};

} // namespace opensu::library::roms
