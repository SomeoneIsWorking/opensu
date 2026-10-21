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

std::optional<BiosScan> biosScanNamed(std::string_view spelling) {
    if (spelling == "pcsx_rearmed") {
        return BiosScan::PcsxRearmed;
    }
    return std::nullopt;
}

std::string CoreBios::mode() const {
    std::string used;
    std::size_t count = 0;
    for (const File& file : files) {
        if (file.used) {
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
    const std::vector<PickedBios> picked =
        scan == BiosScan::PcsxRearmed ? pcsxRearmedBiosIn(system) : std::vector<PickedBios>{};
    for (const CoreFirmware& file : firmware) {
        std::error_code ec;
        const bool present = !system.empty() && fs::is_regular_file(system / file.path, ec);
        CoreBios::File row{file, present, present && scan == BiosScan::Listed, {}};
        if (const auto at = std::ranges::find(picked, file.path, &PickedBios::file);
            at != picked.end()) {
            row.used = true;
            row.region = at->region;
        }
        bios.files.push_back(std::move(row));
    }
    for (const PickedBios& one : picked) {
        if (std::ranges::none_of(firmware, [&one](const CoreFirmware& file) {
                return file.path == one.file;
            })) {
            bios.files.push_back(
                CoreBios::File{{one.file, "Found in the folder", true}, true, true, one.region});
        }
    }
    return bios;
}

} // namespace opensu::library::roms
