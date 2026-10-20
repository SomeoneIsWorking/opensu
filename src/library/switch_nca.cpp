#include "switch_nca.hpp"

#include <algorithm>
#include <climits>

namespace opensu::library::roms {
namespace {

constexpr std::size_t headerSize = 0xC00;
constexpr std::size_t headerSector = 0x200;
constexpr std::size_t magicAt = 0x200;
constexpr std::size_t contentAt = 0x205;
constexpr std::size_t oldGenerationAt = 0x206;
constexpr std::size_t keyIndexAt = 0x207;
constexpr std::size_t programIdAt = 0x210;
constexpr std::size_t generationAt = 0x220;
constexpr std::size_t rightsAt = 0x230;
constexpr std::size_t sectionTableAt = 0x240;
constexpr std::size_t sectionEntry = 0x10;
constexpr std::size_t keyAreaAt = 0x300;
constexpr std::size_t fsHeadersAt = 0x400;
constexpr std::size_t fsHeaderSize = 0x200;
constexpr std::size_t sectionCount = 4;
constexpr std::uint64_t mediaUnit = 0x200;

constexpr std::size_t fsTypeAt = 0x2;
constexpr std::size_t hashTypeAt = 0x3;
constexpr std::size_t encryptionAt = 0x4;
constexpr std::size_t ivfcMagicAt = 0x8;
/// The sixth IVFC level, the RomFS itself: its offset and size in the section.
constexpr std::size_t dataLevelAt = 0x90;
constexpr std::size_t upperCounterAt = 0x140;

constexpr std::uint8_t romfsType = 0;
constexpr std::uint8_t ivfcHash = 3;
constexpr std::uint8_t encryptionNone = 1;
constexpr std::uint8_t encryptionCtr = 3;
constexpr std::size_t ctrKeySlot = 2;

constexpr std::size_t romfsHeader = 0x50;
constexpr std::size_t fileTableAt = 0x38;
constexpr std::size_t fileDataAt = 0x48;
constexpr std::size_t fileEntry = 0x20;
/// A Control NCA's RomFS names a few dozen files; a far larger table is not one.
constexpr std::uint64_t largestFileTable = 0x100000;

bool magicIs(std::span<const std::uint8_t> bytes, std::size_t at, std::string_view magic) {
    return std::equal(magic.begin(), magic.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at),
                      [](char a, std::uint8_t b) {
                          return static_cast<std::uint8_t>(a) == b;
                      });
}

} // namespace

Nca::Nca(SwitchPackage& package, const PackageFile& file) : package_{&package}, file_{file} {
}

std::optional<Nca> Nca::open(SwitchPackage& package, const PackageFile& file,
                             const Key256& headerKey, std::string& error) {
    if (file.size < headerSize) {
        error = file.name + " is too short for an NCA";
        return std::nullopt;
    }
    std::array<std::uint8_t, headerSize> header{};
    if (!package.read(file.offset, header)) {
        error = file.name + " cannot be read";
        return std::nullopt;
    }
    if (!decryptXts(headerKey, 0, headerSector, header)) {
        error = file.name + ": header decryption failed";
        return std::nullopt;
    }
    if (!magicIs(header, magicAt, "NCA3")) {
        error = file.name + ": header is not NCA3 after decryption (header_key wrong, or an older "
                            "NCA format)";
        return std::nullopt;
    }
    Nca nca{package, file};
    nca.content_ = static_cast<NcaContent>(header[contentAt]);
    nca.programId_ = littleEndian<std::uint64_t>(header, programIdAt);
    const std::uint8_t generation = std::max(header[oldGenerationAt], header[generationAt]);
    nca.keyRevision_ = generation == 0 ? 0 : static_cast<std::uint8_t>(generation - 1);
    nca.keyAreaIndex_ = header[keyIndexAt];
    std::copy_n(header.begin() + rightsAt, nca.rights_.size(), nca.rights_.begin());
    std::copy_n(header.begin() + keyAreaAt, nca.keyArea_.size(), nca.keyArea_.begin());
    for (std::size_t i = 0; i < sectionCount; ++i) {
        const std::size_t entry = sectionTableAt + i * sectionEntry;
        const std::uint64_t start = littleEndian<std::uint32_t>(header, entry) * mediaUnit;
        const std::uint64_t end = littleEndian<std::uint32_t>(header, entry + 4) * mediaUnit;
        const std::span<const std::uint8_t> fs{header.data() + fsHeadersAt + i * fsHeaderSize,
                                               fsHeaderSize};
        if (end <= start || end > file.size || fs[fsTypeAt] != romfsType ||
            fs[hashTypeAt] != ivfcHash || !magicIs(fs, ivfcMagicAt, "IVFC")) {
            continue;
        }
        Section section;
        section.start = start;
        section.size = end - start;
        section.encryption = fs[encryptionAt];
        // The counter's upper half is stored little-endian; the block counter goes big-endian.
        std::reverse_copy(fs.begin() + upperCounterAt, fs.begin() + upperCounterAt + 8,
                          section.upperCounter.begin());
        section.romfsAt = littleEndian<std::uint64_t>(fs, dataLevelAt);
        section.romfsSize = littleEndian<std::uint64_t>(fs, dataLevelAt + 8);
        if (section.romfsAt > section.size || section.romfsSize > section.size - section.romfsAt) {
            continue;
        }
        nca.romfs_ = section;
        break;
    }
    wipeBytes(header.data(), header.size());
    return nca;
}

std::optional<Key128> Nca::sectionKey(const SwitchKeys& keys, std::string& error) {
    if (std::ranges::any_of(rights_, [](std::uint8_t b) {
            return b != 0;
        })) {
        std::optional<Key128> titleKey = keys.titleKey(rights_);
        if (!titleKey) {
            titleKey = package_->ticketTitleKey(rights_);
        }
        if (!titleKey) {
            error =
                file_.name + ": no title key for its rights id in title.keys or a common ticket";
            return std::nullopt;
        }
        const std::optional<Key128> kek = keys.titleKek(keyRevision_);
        if (!kek) {
            error = file_.name + ": prod.keys lacks " + SwitchKeys::titleKekName(keyRevision_) +
                    " and the master key it is made from";
            return std::nullopt;
        }
        if (!decryptEcb(*kek, titleKey->bytes())) {
            error = file_.name + ": title key decryption failed";
            return std::nullopt;
        }
        return titleKey;
    }
    if (keyAreaIndex_ > static_cast<std::uint8_t>(KeyArea::System)) {
        error = file_.name + ": unknown key area index";
        return std::nullopt;
    }
    const auto area = static_cast<KeyArea>(keyAreaIndex_);
    const std::optional<Key128> areaKey = keys.keyAreaKey(area, keyRevision_);
    if (!areaKey) {
        error = file_.name + ": prod.keys lacks " + SwitchKeys::keyAreaKeyName(area, keyRevision_) +
                " and the master key it is made from";
        return std::nullopt;
    }
    std::array<std::uint8_t, 0x40> decrypted = keyArea_;
    if (!decryptEcb(*areaKey, decrypted)) {
        error = file_.name + ": key area decryption failed";
        return std::nullopt;
    }
    Key128 key;
    std::copy_n(decrypted.begin() + ctrKeySlot * aesBlock, aesBlock, key.bytes().begin());
    wipeBytes(decrypted.data(), decrypted.size());
    return key;
}

bool Nca::readSection(const Section& section, const Key128* key, std::uint64_t offset,
                      std::span<std::uint8_t> out) {
    if (offset > section.size || out.size() > section.size - offset) {
        return false;
    }
    const std::uint64_t absolute = section.start + offset;
    if (section.encryption == encryptionNone) {
        return package_->read(file_.offset + absolute, out);
    }
    if (key == nullptr) {
        return false;
    }
    const std::uint64_t aligned = absolute - absolute % aesBlock;
    const std::size_t lead = absolute - aligned;
    std::vector<std::uint8_t> buffer(lead + out.size());
    if (!package_->read(file_.offset + aligned, buffer)) {
        return false;
    }
    CounterBlock counter{};
    std::ranges::copy(section.upperCounter, counter.begin());
    std::uint64_t block = aligned / aesBlock;
    for (std::size_t i = counter.size(); i > counter.size() - sizeof(block); --i) {
        counter[i - 1] = static_cast<std::uint8_t>(block & 0xffU);
        block >>= CHAR_BIT;
    }
    if (!decryptCtr(*key, counter, buffer)) {
        return false;
    }
    std::copy_n(buffer.begin() + static_cast<std::ptrdiff_t>(lead), out.size(), out.begin());
    return true;
}

bool Nca::listRomFs(const SwitchKeys& keys, std::vector<RomFsFile>& out, std::string& error) {
    if (!romfs_) {
        error = file_.name + " has no RomFS section";
        return false;
    }
    const Section& section = *romfs_;
    if (section.encryption != encryptionNone && section.encryption != encryptionCtr) {
        error = file_.name + ": RomFS encryption type " + std::to_string(section.encryption) +
                " is not read";
        return false;
    }
    if (section.encryption == encryptionCtr && !key_) {
        key_ = sectionKey(keys, error);
        if (!key_) {
            return false;
        }
    }
    const Key128* key = key_ ? &*key_ : nullptr;
    std::array<std::uint8_t, romfsHeader> header{};
    if (!readSection(section, key, section.romfsAt, header) ||
        littleEndian<std::uint64_t>(header, 0) != romfsHeader) {
        error = file_.name + ": RomFS header does not read (wrong key?)";
        return false;
    }
    const auto tableAt = littleEndian<std::uint64_t>(header, fileTableAt);
    const auto tableSize = littleEndian<std::uint64_t>(header, fileTableAt + 8);
    const auto dataAt = littleEndian<std::uint64_t>(header, fileDataAt);
    std::vector<std::uint8_t> table;
    if (tableSize <= largestFileTable && tableAt <= section.romfsSize &&
        tableSize <= section.romfsSize - tableAt) {
        table.resize(tableSize);
    }
    if (table.empty() || !readSection(section, key, section.romfsAt + tableAt, table)) {
        error = file_.name + ": RomFS file table does not read";
        return false;
    }
    for (std::size_t at = 0; at + fileEntry <= table.size();) {
        const auto nameSize = littleEndian<std::uint32_t>(table, at + 0x1C);
        if (nameSize > table.size() - at - fileEntry) {
            break;
        }
        RomFsFile file;
        file.offset = dataAt + littleEndian<std::uint64_t>(table, at + 0x8);
        file.size = littleEndian<std::uint64_t>(table, at + 0x10);
        file.name.assign(table.begin() + static_cast<std::ptrdiff_t>(at + fileEntry),
                         table.begin() + static_cast<std::ptrdiff_t>(at + fileEntry + nameSize));
        out.push_back(std::move(file));
        at += fileEntry + ((nameSize + 3U) & ~std::size_t{3});
    }
    return true;
}

std::optional<std::string> Nca::readRomFs(const RomFsFile& file, std::uint64_t largest) {
    if (!romfs_) {
        return std::nullopt;
    }
    const Section& section = *romfs_;
    if (file.size > largest || file.offset > section.romfsSize ||
        file.size > section.romfsSize - file.offset) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes(file.size);
    if (!readSection(section, key_ ? &*key_ : nullptr, section.romfsAt + file.offset, bytes)) {
        return std::nullopt;
    }
    return std::string{bytes.begin(), bytes.end()};
}

} // namespace opensu::library::roms
