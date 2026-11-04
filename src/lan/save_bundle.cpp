#include "save_bundle.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "clock/file_time.hpp"
#include "library/save_copy.hpp"
#include "peer_crypto.hpp"
#include "secrets/secret.hpp"
#include "wire.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;
using library::saves::EmulatorSaves;
using library::saves::SaveSlot;
using library::saves::SlotKind;
using nlohmann::json;
using TimePoint = std::chrono::system_clock::time_point;

constexpr std::array kindKeys{
    std::pair{SlotKind::Account, "account"}, std::pair{SlotKind::Device, "device"},
    std::pair{SlotKind::Memory, "memory"}, std::pair{SlotKind::State, "state"}};
/// A header names a few thousand files at most.
constexpr std::size_t largestHeader = std::size_t{16} * 1024 * 1024;

std::string_view keyOf(SlotKind kind) {
    const auto found = std::ranges::find(kindKeys, kind, &std::pair<SlotKind, const char*>::first);
    return found->second;
}

std::optional<SlotKind> kindOf(std::string_view key) {
    const auto found =
        std::ranges::find_if(kindKeys, [key](const std::pair<SlotKind, const char*>& entry) {
            return key == entry.second;
        });
    if (found == kindKeys.end()) {
        return std::nullopt;
    }
    return found->first;
}

TimePoint modifiedOf(const fs::path& file) {
    return TimePoint{std::chrono::duration_cast<TimePoint::duration>(
        clock::sinceEpoch(fs::last_write_time(file)))};
}

std::optional<std::string> readWhole(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (!in.good() && !in.eof()) {
        return std::nullopt;
    }
    return bytes;
}

/// Whether `path` stays inside the slot it is written under: relative, no empty, `.` or `..`
/// step, no backslash, drive colon or NUL.
bool insideSlot(std::string_view path) {
    if (path.empty()) {
        return true;
    }
    if (path.front() == '/' ||
        path.find_first_of(std::string_view{"\\:\0", 3}) != std::string_view::npos) {
        return false;
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string_view step = path.substr(start, end - start);
        if (step.empty() || step == "." || step == "..") {
            return false;
        }
        start = end + 1;
    }
    return true;
}

bool sameFile(const BundleFile& a, const BundleFile& b) {
    return a.kind == b.kind && a.path == b.path;
}

bool before(const BundleFile& a, const BundleFile& b) {
    return a.kind != b.kind ? a.kind < b.kind : a.path < b.path;
}

} // namespace

std::string SaveBundle::hash() const {
    std::vector<const BundleFile*> ordered;
    ordered.reserve(files.size());
    for (const BundleFile& file : files) {
        ordered.push_back(&file);
    }
    std::ranges::sort(ordered, [](const BundleFile* a, const BundleFile* b) {
        return before(*a, *b);
    });
    std::string listing = "opensu-saves\n";
    for (const BundleFile* file : ordered) {
        listing += keyOf(file->kind);
        listing += '\n';
        listing += file->path;
        listing += '\n';
        listing += secrets::hexText(crypto::sha256(file->bytes));
        listing += '\n';
    }
    return secrets::hexText(crypto::sha256(listing));
}

std::uintmax_t SaveBundle::bytes() const {
    std::uintmax_t total = 0;
    for (const BundleFile& file : files) {
        total += file.bytes.size();
    }
    return total;
}

std::optional<TimePoint> SaveBundle::modified() const {
    std::optional<TimePoint> newest;
    for (const BundleFile& file : files) {
        if (!newest || file.modified > *newest) {
            newest = file.modified;
        }
    }
    return newest;
}

std::optional<SaveBundle> bundleOf(const EmulatorSaves& saves, std::string& error) {
    SaveBundle bundle;
    std::uintmax_t total = 0;
    const auto add = [&](SlotKind kind, const fs::path& file, std::string path) {
        std::optional<std::string> bytes = readWhole(file);
        if (!bytes) {
            error = file.string() + " cannot be read";
            return false;
        }
        total += bytes->size();
        if (total > largestBundle) {
            error = saves.emulatorName + "'s saves of this game are too large to send";
            return false;
        }
        bundle.files.push_back(BundleFile{.kind = kind,
                                          .path = std::move(path),
                                          .bytes = std::move(*bytes),
                                          .modified = modifiedOf(file)});
        return true;
    };
    try {
        for (const SaveSlot* slot : library::saves::copiedSlots(saves)) {
            const fs::path& root = slot->content.front();
            if (fs::is_regular_file(root)) {
                if (!add(slot->kind, root, {})) {
                    return std::nullopt;
                }
                continue;
            }
            for (const fs::directory_entry& entry : fs::recursive_directory_iterator{root}) {
                if (entry.is_regular_file() &&
                    !add(slot->kind, entry.path(),
                         entry.path().lexically_relative(root).generic_string())) {
                    return std::nullopt;
                }
            }
        }
    } catch (const fs::filesystem_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
    return bundle;
}

std::string write(const SaveBundle& bundle) {
    json files = json::array();
    for (const BundleFile& file : bundle.files) {
        files.push_back(
            {{"kind", keyOf(file.kind)},
             {"path", file.path},
             {"bytes", file.bytes.size()},
             {"modified",
              std::chrono::floor<std::chrono::seconds>(file.modified).time_since_epoch().count()}});
    }
    const std::string header = json{{"files", files}}.dump();
    std::string out = std::to_string(header.size()) + "\n" + header;
    for (const BundleFile& file : bundle.files) {
        out += file.bytes;
    }
    return out;
}

std::optional<SaveBundle> readBundle(std::string_view bytes) {
    const std::size_t line = bytes.find('\n');
    const std::string_view digits = bytes.substr(0, line);
    std::size_t length = 0;
    if (line == std::string_view::npos ||
        std::from_chars(digits.data(), digits.data() + digits.size(), length).ptr !=
            digits.data() + digits.size() ||
        length > largestHeader || length > bytes.size() - line - 1) {
        return std::nullopt;
    }
    const json header = json::parse(bytes.substr(line + 1, length), nullptr, false);
    if (!header.is_object() || !header.contains("files") || !header["files"].is_array()) {
        return std::nullopt;
    }
    std::size_t at = line + 1 + length;
    SaveBundle bundle;
    for (const json& entry : header["files"]) {
        if (!entry.is_object() || !entry.contains("bytes") ||
            !entry["bytes"].is_number_unsigned() || !entry.contains("modified") ||
            !entry["modified"].is_number_integer()) {
            return std::nullopt;
        }
        const std::optional<SlotKind> kind = kindOf(wire::textField(entry, "kind"));
        std::string path = wire::textField(entry, "path");
        const auto size = entry["bytes"].get<std::uint64_t>();
        if (!kind || !insideSlot(path) || size > bytes.size() - at) {
            return std::nullopt;
        }
        BundleFile file{.kind = *kind,
                        .path = std::move(path),
                        .bytes = std::string{bytes.substr(at, size)},
                        .modified =
                            TimePoint{std::chrono::seconds{entry["modified"].get<std::int64_t>()}}};
        at += size;
        // A slot is one file or a folder of them, each named once.
        const bool clash = std::ranges::any_of(bundle.files, [&file](const BundleFile& kept) {
            return kept.kind == file.kind &&
                   (sameFile(kept, file) || kept.path.empty() || file.path.empty());
        });
        if (clash) {
            return std::nullopt;
        }
        bundle.files.push_back(std::move(file));
    }
    if (at != bytes.size()) {
        return std::nullopt;
    }
    return bundle;
}

EmulatorSaves staged(const SaveBundle& bundle, const fs::path& folder, std::string name) {
    fs::remove_all(folder);
    EmulatorSaves saves{.emulatorId = {},
                        .emulatorName = std::move(name),
                        .slots = {},
                        .home = std::nullopt,
                        .processHint = {}};
    for (const auto& [kind, key] : kindKeys) {
        const fs::path slotFolder = folder / key;
        std::optional<SaveSlot> slot;
        for (const BundleFile& file : bundle.files) {
            if (file.kind != kind) {
                continue;
            }
            const fs::path target =
                file.path.empty() ? slotFolder : slotFolder / fs::path{file.path};
            fs::create_directories(target.parent_path());
            std::ofstream out{target, std::ios::binary | std::ios::trunc};
            out.write(file.bytes.data(), static_cast<std::streamsize>(file.bytes.size()));
            out.close();
            if (!out) {
                throw fs::filesystem_error{"cannot write", target,
                                           std::make_error_code(std::errc::io_error)};
            }
            fs::last_write_time(target, clock::fileTimeOf(file.modified));
            if (!slot) {
                slot = SaveSlot{.kind = kind,
                                .user = {},
                                .current = true,
                                .root = slotFolder,
                                .content = {slotFolder},
                                .bytes = 0,
                                .modified = std::nullopt};
            }
            slot->bytes += file.bytes.size();
            if (!slot->modified || file.modified > *slot->modified) {
                slot->modified = file.modified;
            }
        }
        if (slot) {
            saves.slots.push_back(std::move(*slot));
        }
    }
    return saves;
}

} // namespace opensu::lan
