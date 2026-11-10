#include "core_bios.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <map>
#include <system_error>

#include "zip/zip_archive.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

/// The whole archive is a few hundred kilobytes; one info file a few kilobytes.
constexpr std::uint32_t maxInfoBytes = 1024U * 1024U;
constexpr std::string_view firmwarePrefix = "firmware";

std::string_view trimmed(std::string_view text) {
    const std::size_t start = text.find_first_not_of(" \t\r");
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r");
    return text.substr(start, end - start + 1);
}

/// The `key = "value"` lines of an info file; comments and other lines are skipped.
std::map<std::string, std::string, std::less<>> valuesIn(std::string_view info) {
    std::map<std::string, std::string, std::less<>> values;
    std::size_t at = 0;
    while (at < info.size()) {
        const std::size_t end = std::min(info.find('\n', at), info.size());
        const std::string_view line = trimmed(info.substr(at, end - at));
        at = end + 1;
        const std::size_t equals = line.find('=');
        if (line.starts_with('#') || equals == std::string_view::npos) {
            continue;
        }
        std::string_view value = trimmed(line.substr(equals + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        values.insert_or_assign(std::string{trimmed(line.substr(0, equals))}, std::string{value});
    }
    return values;
}

std::string valueOf(const std::map<std::string, std::string, std::less<>>& values,
                    const std::string& key) {
    const auto found = values.find(key);
    return found == values.end() ? std::string{} : found->second;
}

/// PCSX ReARMed's `loadPSXBios`: the names it tries first, in order, each with `.bin`.
constexpr std::array<std::string_view, 7> pcsxListedBios{
    "scph5500", "scph5501", "scph5502", "psxonpsp660", "scph101", "scph7001", "scph1001"};
/// Its regions, in `PSX_REGION_*` order.
constexpr std::size_t regionCount = 3;
constexpr std::array<std::string_view, regionCount> psxRegionNames{"US", "JP", "EU"};
constexpr std::size_t psxRegionUs = 0;
constexpr std::size_t psxRegionJp = 1;
constexpr std::size_t psxRegionEu = 2;
/// `try_use_bios` takes only 512 KiB files (both callers pass `preferred_only`).
constexpr std::size_t psxBiosBytes = std::size_t{512} * 1024;
constexpr std::string_view psxResetSignature{"\x00\x08\x3c\x3f", 4};
constexpr std::size_t psxResetSignatureAt = 1;
constexpr std::string_view psxCompatible = "PS compatible";
constexpr std::size_t psxCompatibleAt = 0x12c;
constexpr std::size_t psxRegionMarkAt = 0x7ff51;
/// Japanese BIOSes `try_use_bios` knows by CRC-32 instead of their region mark.
constexpr std::array<std::uint32_t, 3> psxJapaneseCrcs{0x18D0F7D8, 0x3B601FC8, 0x3539DEF6};

bool containsNoCase(std::string_view text, std::string_view part) {
    const auto lower = [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    };
    std::string folded;
    std::ranges::transform(text, std::back_inserter(folded), lower);
    std::string wanted;
    std::ranges::transform(part, std::back_inserter(wanted), lower);
    return folded.find(wanted) != std::string::npos;
}

/// The region `try_use_bios` files `file` under, or nothing when it skips the file.
std::optional<std::size_t> psxRegionOf(const fs::path& file) {
    if (containsNoCase(file.filename().string(), "unirom")) {
        return std::nullopt;
    }
    std::error_code ec;
    if (!fs::is_regular_file(file, ec) || fs::file_size(file, ec) != psxBiosBytes || ec) {
        return std::nullopt;
    }
    std::ifstream in{file, std::ios::binary};
    std::string data(psxBiosBytes, '\0');
    if (!in.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        return std::nullopt;
    }
    const std::string_view bytes{data};
    if (bytes.substr(psxResetSignatureAt, psxResetSignature.size()) != psxResetSignature &&
        bytes.substr(psxCompatibleAt, psxCompatible.size()) != psxCompatible) {
        return std::nullopt;
    }
    const std::string_view mark = bytes.substr(psxRegionMarkAt, 2);
    if (mark == " E") {
        return psxRegionEu;
    }
    if (mark == " J" ||
        std::ranges::find(psxJapaneseCrcs, zip::crc32Of(bytes)) != psxJapaneseCrcs.end()) {
        return psxRegionJp;
    }
    return psxRegionUs;
}

/// LRPS2's `IsBIOS` takes files of 4 to 8 MiB.
constexpr std::uintmax_t ps2BiosMinBytes = std::uintmax_t{4} * 1024 * 1024;
constexpr std::uintmax_t ps2BiosMaxBytes = std::uintmax_t{8} * 1024 * 1024;
/// A ROM directory entry: a 10-byte name, a 2-byte extended info size, a 4-byte file size.
constexpr std::size_t romdirEntryBytes = 16;
constexpr std::size_t romdirNameBytes = 10;
constexpr std::size_t romdirSizeAt = 12;
/// ROMVER's text: `0160AC20030627`, version, region letter, console or devel, date.
constexpr std::size_t romverBytes = 14;
constexpr std::size_t romverRegionAt = 4;
constexpr std::size_t romverKindAt = 5;
constexpr std::uint32_t romdirAlignment = 0x10;

/// A ROM directory entry's name, up to its first NUL.
std::string_view romdirName(std::string_view entry) {
    const std::string_view name = entry.substr(0, romdirNameBytes);
    return name.substr(0, std::min(name.find('\0'), name.size()));
}

std::uint32_t romdirSize(std::string_view entry) {
    std::uint32_t size = 0;
    for (std::size_t at = 0; at < 4; ++at) {
        size |= static_cast<std::uint32_t>(static_cast<unsigned char>(entry[romdirSizeAt + at]))
                << (8 * at);
    }
    return size;
}

/// The zone `LoadBiosVersion` reads from ROMVER's region letter.
std::string ps2ZoneOf(char region, char kind) {
    switch (region) {
    case 'J':
        return "Japan";
    case 'A':
        return "USA";
    case 'E':
        return "Europe";
    case 'H':
        return "Asia";
    case 'C':
        return "China";
    case 'T':
        return kind == 'Z' ? "COH-H" : "T10K";
    case 'X':
        return "Test";
    case 'P':
        return "Free";
    default:
        return std::string(1, region);
    }
}

/// The zone LRPS2's `IsBIOS` gives `file`, or nothing when it is not taken: its ROM directory is
/// found by the `RESET` entry on a 16-byte boundary and must hold a `ROMVER` entry.
std::optional<std::string> ps2ZoneIn(const fs::path& file) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) {
        return std::nullopt;
    }
    const std::uintmax_t size = fs::file_size(file, ec);
    if (ec || size < ps2BiosMinBytes || size > ps2BiosMaxBytes) {
        return std::nullopt;
    }
    std::ifstream in{file, std::ios::binary};
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!in.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        return std::nullopt;
    }
    const std::string_view bytes{data};
    std::size_t at = 0;
    while (at + romdirEntryBytes <= bytes.size() &&
           romdirName(bytes.substr(at, romdirEntryBytes)) != "RESET") {
        at += romdirEntryBytes;
    }
    std::uint64_t offset = 0;
    for (; at + romdirEntryBytes <= bytes.size(); at += romdirEntryBytes) {
        const std::string_view entry = bytes.substr(at, romdirEntryBytes);
        const std::string_view name = romdirName(entry);
        if (name.empty() || name.size() == romdirNameBytes) {
            break;
        }
        if (name == "ROMVER") {
            if (offset + romverBytes > bytes.size()) {
                return std::nullopt;
            }
            const std::string_view romver = bytes.substr(offset, romverBytes);
            return ps2ZoneOf(romver[romverRegionAt], romver[romverKindAt]);
        }
        const std::uint32_t entrySize = romdirSize(entry);
        offset += (entrySize % romdirAlignment) == 0
                      ? entrySize
                      : (std::uint64_t{entrySize} + romdirAlignment) & ~std::uint64_t{0xf};
    }
    return std::nullopt;
}

/// `description` without the file name libretro repeats in it: "PS1 US BIOS" of
/// "scph5501.bin (PS1 US BIOS)".
std::string describedWithout(std::string description, std::string_view path) {
    const std::string named = std::string{path} + " (";
    if (description.starts_with(named) && description.ends_with(')')) {
        return description.substr(named.size(), description.size() - named.size() - 1);
    }
    return description;
}

} // namespace

std::optional<WithoutBios> withoutBiosNamed(std::string_view spelling) {
    if (spelling == "needed") {
        return WithoutBios::Needed;
    }
    if (spelling == "hle") {
        return WithoutBios::Hle;
    }
    if (spelling == "open") {
        return WithoutBios::Open;
    }
    if (spelling == "skipped") {
        return WithoutBios::Skipped;
    }
    return std::nullopt;
}

std::string_view label(WithoutBios mode) noexcept {
    switch (mode) {
    case WithoutBios::Needed:
        return "Missing";
    case WithoutBios::Hle:
        return "HLE built in";
    case WithoutBios::Open:
        return "Open BIOS built in";
    case WithoutBios::Skipped:
        return "Not needed";
    }
    return "Missing";
}

std::vector<CoreFirmware> firmwareIn(std::string_view info) {
    const auto values = valuesIn(info);
    const std::string countText = valueOf(values, "firmware_count");
    std::size_t count = 0;
    const auto [end, ec] =
        std::from_chars(countText.data(), countText.data() + countText.size(), count);
    if (ec != std::errc{} || end != countText.data() + countText.size()) {
        return {};
    }
    std::vector<CoreFirmware> firmware;
    for (std::size_t index = 0; index < count; ++index) {
        const std::string key = std::string{firmwarePrefix} + std::to_string(index);
        const std::string path = valueOf(values, key + "_path");
        CoreFirmware file{path, describedWithout(valueOf(values, key + "_desc"), path),
                          valueOf(values, key + "_opt") == "true"};
        const fs::path relative{file.path};
        if (file.path.empty() || relative.is_absolute() ||
            std::ranges::any_of(relative, [](const fs::path& part) {
                return part == "..";
            })) {
            continue;
        }
        firmware.push_back(std::move(file));
    }
    return firmware;
}

std::optional<CoreInfoArchive> CoreInfoArchive::open(std::string zip, std::string& error) {
    std::optional<zip::Archive> archive = zip::Archive::open(std::move(zip), error);
    if (!archive) {
        error = "the core info archive is not a zip: " + error;
        return std::nullopt;
    }
    return CoreInfoArchive{std::move(*archive)};
}

std::optional<std::string> CoreInfoArchive::info(std::string_view core, std::string& error) const {
    const std::string name = std::string{core} + std::string{coreInfoExtension};
    const bool listed = std::ranges::any_of(archive_.entries(), [&name](const zip::Entry& entry) {
        return entry.name == name;
    });
    if (!listed) {
        error = "the core info archive has no " + name;
        return std::nullopt;
    }
    std::optional<std::string> text = archive_.read(name, error, maxInfoBytes);
    if (!text) {
        error = name + " cannot be taken out of the core info archive: " + error;
    }
    return text;
}

std::vector<PickedBios> pcsxRearmedBiosIn(const fs::path& system) {
    std::array<std::optional<std::string>, regionCount> saved;
    const auto all = [&saved] {
        return std::ranges::all_of(saved, [](const auto& file) {
            return file.has_value();
        });
    };
    std::vector<PickedBios> picked;
    const auto tryFile = [&saved, &picked](const fs::path& file) {
        const std::string name = file.filename().string();
        if (std::ranges::any_of(saved, [&name](const auto& kept) {
                return kept == name;
            })) {
            return;
        }
        const std::optional<std::size_t> region = psxRegionOf(file);
        if (region && !saved.at(*region)) {
            saved.at(*region) = name;
            picked.push_back(PickedBios{name, std::string{psxRegionNames.at(*region)}});
        }
    };
    bool allByName = false;
    for (const std::string_view name : pcsxListedBios) {
        tryFile(system / (std::string{name} + ".bin"));
        if (all()) {
            allByName = true;
            break;
        }
    }
    std::error_code ec;
    if (allByName || !fs::is_directory(system, ec)) {
        return picked;
    }
    // The core reads the folder in readdir order, as directory_iterator does.
    for (const fs::directory_entry& entry : fs::directory_iterator{system, ec}) {
        tryFile(entry.path());
        if (all()) {
            break;
        }
    }
    return picked;
}

std::vector<PickedBios> lrps2BiosIn(const fs::path& system) {
    std::vector<PickedBios> picked;
    const fs::path folder = system / lrps2BiosFolder;
    std::error_code ec;
    if (system.empty() || !fs::is_directory(folder, ec)) {
        return picked;
    }
    // The core lists the folder in readdir order, as directory_iterator does.
    for (const fs::directory_entry& entry : fs::directory_iterator{folder, ec}) {
        if (const std::optional<std::string> zone = ps2ZoneIn(entry.path())) {
            picked.push_back(PickedBios{
                (fs::path{lrps2BiosFolder} / entry.path().filename()).generic_string(), *zone});
        }
    }
    return picked;
}

std::optional<BiosScan> biosScanNamed(std::string_view spelling) {
    if (spelling == "pcsx_rearmed") {
        return BiosScan::PcsxRearmed;
    }
    if (spelling == "lrps2") {
        return BiosScan::Lrps2;
    }
    return std::nullopt;
}

std::string CoreBios::mode() const {
    std::string used;
    std::size_t count = 0;
    const auto holdsUsed = [this](const File& folder) {
        const std::string inside = folder.firmware.path + "/";
        return std::ranges::any_of(files, [&inside](const File& file) {
            return file.used && file.firmware.path.starts_with(inside);
        });
    };
    for (const File& file : files) {
        // A listed folder is named by the file in it the core boots.
        if (file.used && !holdsUsed(file)) {
            used += (used.empty() ? "" : ", ") + file.firmware.path;
            ++count;
        }
    }
    if (count == 0) {
        return std::string{label(without)};
    }
    return (count == 1 ? "Your BIOS file " : "Your BIOS files ") + used;
}

CoreBios biosIn(const std::vector<CoreFirmware>& firmware, WithoutBios without, BiosScan scan,
                const fs::path& system) {
    CoreBios bios{without, {}};
    const std::vector<PickedBios> picked = scan == BiosScan::PcsxRearmed ? pcsxRearmedBiosIn(system)
                                           : scan == BiosScan::Lrps2 ? lrps2BiosIn(system)
                                                                     : std::vector<PickedBios>{};
    // PCSX ReARMed boots one file per region; LRPS2 the first it lists.
    const auto boots = [&picked, scan](const PickedBios& one) {
        return scan != BiosScan::Lrps2 || &one == &picked.front();
    };
    for (const CoreFirmware& file : firmware) {
        std::error_code ec;
        const bool present = !system.empty() && fs::is_regular_file(system / file.path, ec);
        CoreBios::File row{file, present, present && scan == BiosScan::Listed, {}};
        if (const auto at = std::ranges::find(picked, file.path, &PickedBios::file);
            at != picked.end()) {
            row.present = true;
            row.used = boots(*at);
            row.region = at->region;
        } else if (scan == BiosScan::Lrps2 && file.path == lrps2BiosFolder && !picked.empty()) {
            row.present = true;
            row.used = true;
            row.region = picked.front().region;
        }
        bios.files.push_back(std::move(row));
    }
    for (const PickedBios& one : picked) {
        if (std::ranges::none_of(firmware, [&one](const CoreFirmware& file) {
                return file.path == one.file;
            })) {
            bios.files.push_back(CoreBios::File{
                {one.file, "Found in the folder", true}, true, boots(one), one.region});
        }
    }
    return bios;
}

} // namespace opensu::library::roms
