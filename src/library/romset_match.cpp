#include "romset_match.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "zip/zip_archive.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

/// The end record and the longest comment a zip may have after it.
constexpr std::size_t maxTail = 22 + 65535;
/// A central directory past this is no romset's.
constexpr std::uint32_t maxDirectory = 16U * 1024 * 1024;
constexpr std::size_t namedMissing = 4;
constexpr int keptVersion = 2;
constexpr std::string_view zipExtension = ".zip";

std::string_view fitName(RomsetFit fit) {
    switch (fit) {
    case RomsetFit::Matches:
        return "matches";
    case RomsetFit::Mismatch:
        return "mismatch";
    case RomsetFit::Pending:
        return "pending";
    case RomsetFit::Unchecked:
        return "unchecked";
    }
    return "unchecked";
}

RomsetFit fitOf(std::string_view name) {
    if (name == "matches") {
        return RomsetFit::Matches;
    }
    return name == "mismatch" ? RomsetFit::Mismatch : RomsetFit::Unchecked;
}

json stampJson(const FileStamp& stamp) {
    return json::array({stamp.path, stamp.size, stamp.modified});
}

FileStamp stampFrom(const json& value) {
    return FileStamp{value.at(0).get<std::string>(), value.at(1).get<std::int64_t>(),
                     value.at(2).get<std::int64_t>()};
}

std::string lower(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// The default BIOS choice along a set's chain: its own, else its parent's or BIOS's.
std::string defaultBiosOf(const std::vector<std::string>& chain, const RomsetDat& dat) {
    for (const std::string& name : chain) {
        if (const DatLink* link = dat.link(name); link != nullptr && !link->defaultBios.empty()) {
            return link->defaultBios;
        }
    }
    return {};
}

/// The ROMs `set` and its devices need, and the devices' names.
void addNeeds(const DatSet& set, const RomsetDat& dat, const std::string& bios,
              std::vector<const DatRom*>& roms, std::set<std::string, std::less<>>& devices) {
    for (const DatRom& rom : set.roms) {
        if (rom.bios.empty() || rom.bios == bios) {
            roms.push_back(&rom);
        }
    }
    for (const std::string& name : set.devices) {
        if (const DatSet* device = dat.find(name);
            device != nullptr && device->device && devices.insert(name).second) {
            addNeeds(*device, dat, {}, roms, devices);
        }
    }
}

} // namespace

std::optional<std::vector<ZipMember>> readZipMembers(RomFile& zip, std::string_view label,
                                                     std::string& error) {
    const std::string name{label};
    const std::uint64_t size = zip.size();
    const std::size_t tailSize = static_cast<std::size_t>(std::min<std::uint64_t>(size, maxTail));
    const std::uint64_t tailOffset = size - tailSize;
    const std::optional<std::string> tail = zip.read(tailOffset, tailSize);
    if (!tail) {
        error = name + " cannot be read";
        return std::nullopt;
    }
    const std::optional<zip::Directory> directory = zip::findDirectory(*tail, tailOffset, error);
    if (!directory) {
        error = name + ": " + error;
        return std::nullopt;
    }
    if (directory->size > maxDirectory ||
        std::uint64_t{directory->offset} + directory->size > size) {
        error = name + ": the central directory is out of the file";
        return std::nullopt;
    }
    const std::optional<std::string> bytes = zip.read(directory->offset, directory->size);
    if (!bytes) {
        error = name + "'s central directory cannot be read";
        return std::nullopt;
    }
    const std::optional<std::vector<zip::Entry>> entries =
        zip::parseDirectory(*bytes, *directory, error);
    if (!entries) {
        error = name + ": " + error;
        return std::nullopt;
    }
    std::vector<ZipMember> members;
    members.reserve(entries->size());
    for (const zip::Entry& entry : *entries) {
        members.push_back(ZipMember{entry.name, entry.size, entry.crc32});
    }
    return members;
}

std::string romsetOf(std::string_view fileName) {
    std::string name = lower(fileName);
    if (!name.ends_with(zipExtension)) {
        return {};
    }
    name.resize(name.size() - zipExtension.size());
    return name;
}

RomFolder::RomFolder(const RomTree& tree, fs::path location, std::span<const RomNode> entries)
    : tree_{&tree}, location_{std::move(location)} {
    for (const RomNode& entry : entries) {
        entries_.emplace(lower(entry.name), &entry);
    }
}

const RomNode* RomFolder::find(std::string_view name) const {
    const auto found = entries_.find(name);
    return found == entries_.end() ? nullptr : found->second;
}

RomsetDat::Wanted RomFolder::sets() const {
    RomsetDat::Wanted sets;
    for (const auto& [name, entry] : entries_) {
        if (std::string set = romsetOf(name); !entry->folder && !set.empty()) {
            sets.insert(std::move(set));
        }
    }
    return sets;
}

const RomNode* FolderZips::nodeOf(std::string_view relative) {
    const std::size_t slash = relative.find('/');
    if (slash == std::string_view::npos) {
        return folder_->find(relative);
    }
    const std::string_view inner = relative.substr(0, slash);
    auto listed = subfolders_.find(inner);
    if (listed == subfolders_.end()) {
        const RomNode* node = folder_->find(inner);
        std::vector<RomNode> entries;
        if (node != nullptr && node->folder) {
            entries = folder_->tree().children(node->location).value_or(std::vector<RomNode>{});
        }
        listed = subfolders_.emplace(std::string{inner}, std::move(entries)).first;
    }
    const auto file = std::ranges::find_if(listed->second, [&](const RomNode& entry) {
        return lower(entry.name) == relative.substr(slash + 1);
    });
    return file == listed->second.end() ? nullptr : &*file;
}

FileStamp FolderZips::stampOf(std::string_view relative) {
    const std::string path = lower(relative);
    const RomNode* node = nodeOf(path);
    if (node == nullptr) {
        return FileStamp{path, -1, 0};
    }
    return FileStamp{path, static_cast<std::int64_t>(node->size), node->modified};
}

const std::vector<ZipMember>* FolderZips::members(std::string_view set) {
    auto found = zips_.find(set);
    if (found == zips_.end()) {
        const std::string zip = std::string{set} + std::string{zipExtension};
        FileStamp stamp = stampOf(zip);
        std::optional<std::vector<ZipMember>> members;
        if (stamp.size >= 0) {
            std::string error = zip + " cannot be opened";
            if (const std::unique_ptr<RomFile> file = folder_->tree().open(nodeOf(zip)->location)) {
                members = readZipMembers(*file, zip, error);
            }
            if (!members) {
                lucent::warn("roms", "{}", error);
            }
        }
        looked_.push_back(std::move(stamp));
        found = zips_.emplace(std::string{set}, std::move(members)).first;
    }
    return found->second ? &*found->second : nullptr;
}

bool FolderZips::hasDisk(std::string_view set, std::string_view disk) {
    FileStamp stamp = stampOf(std::string{set} + "/" + std::string{disk} + ".chd");
    const bool there = stamp.size >= 0;
    if (std::ranges::find(looked_, stamp) == looked_.end()) {
        looked_.push_back(std::move(stamp));
    }
    return there;
}

RomsetVerdict matchSet(std::string_view name, const RomsetDat& dat, FolderZips& zips) {
    RomsetVerdict verdict;
    verdict.fit = RomsetFit::Mismatch;
    const DatSet* set = dat.find(name);
    if (set == nullptr) {
        return verdict;
    }
    verdict.listed = true;
    const std::vector<std::string> chain = dat.chainOf(set->name);
    std::vector<const DatRom*> needs;
    std::set<std::string, std::less<>> devices;
    addNeeds(*set, dat, defaultBiosOf(chain, dat), needs, devices);

    std::set<std::pair<std::uint32_t, std::uint64_t>> held;
    const auto hold = [&](std::string_view zip) {
        if (const std::vector<ZipMember>* members = zips.members(zip)) {
            for (const ZipMember& member : *members) {
                held.emplace(member.crc32, member.size);
            }
            if (zip != name) {
                verdict.zips.emplace_back(zip);
            }
        }
    };
    std::ranges::for_each(chain, hold);
    std::ranges::for_each(devices, hold);

    std::set<std::pair<std::uint32_t, std::uint64_t>> counted;
    bool offersChoice = false;
    bool holdsChoice = false;
    for (const DatRom* rom : needs) {
        const std::pair key{rom->crc32, rom->size};
        if (rom->choice) {
            offersChoice = true;
            holdsChoice = holdsChoice || held.contains(key);
            continue;
        }
        if (!counted.insert(key).second) {
            continue;
        }
        ++verdict.required;
        if (!held.contains(key)) {
            verdict.missing.push_back(rom->name);
        }
    }
    if (offersChoice) {
        ++verdict.required;
        if (!holdsChoice) {
            verdict.missing.push_back(chain.back() + std::string{zipExtension});
        }
    }
    for (const std::string& disk : set->disks) {
        ++verdict.required;
        if (std::ranges::none_of(chain, [&](const std::string& owner) {
                return zips.hasDisk(owner, disk);
            })) {
            verdict.missing.push_back(disk + ".chd");
        }
    }
    verdict.fit = verdict.missing.empty() ? RomsetFit::Matches : RomsetFit::Mismatch;
    return verdict;
}

std::string gapOf(const RomsetVerdict& verdict, std::string_view set) {
    if (!verdict.listed) {
        return "has no set " + std::string{set};
    }
    std::string text =
        std::format("lacks {} of {} files: ", verdict.missing.size(), verdict.required);
    const std::size_t named = std::min(verdict.missing.size(), namedMissing);
    for (std::size_t i = 0; i < named; ++i) {
        text += (i == 0 ? "" : ", ") + verdict.missing[i];
    }
    if (verdict.missing.size() > named) {
        text += std::format(" and {} more", verdict.missing.size() - named);
    }
    return text;
}

void addBesideZips(Game& game, const RomFolder& folder,
                   const std::vector<RomsetVerdict>& verdicts) {
    for (const RomsetVerdict& verdict : verdicts) {
        const auto option =
            std::ranges::find(game.emulatorOptions, verdict.emulatorId, &EmulatorOption::id);
        if (option == game.emulatorOptions.end() || !option->installed ||
            verdict.fit != RomsetFit::Matches) {
            continue;
        }
        for (const std::string& set : verdict.zips) {
            const RomNode* zip = folder.find(set + std::string{zipExtension});
            if (zip == nullptr || zip->local) {
                continue;
            }
            const std::vector<std::string> beside{std::string{besideArgument},
                                                  zip->location.string()};
            option->launch.args.insert(option->launch.args.begin(), beside.begin(), beside.end());
            if (option->name == game.emulator) {
                game.launch.args.insert(game.launch.args.begin(), beside.begin(), beside.end());
            }
        }
    }
}

void applyRomsets(Game& game, std::string_view set, const std::vector<RomsetVerdict>& verdicts) {
    if (std::ranges::all_of(verdicts, [](const RomsetVerdict& verdict) {
            return verdict.fit == RomsetFit::Unchecked;
        })) {
        return;
    }
    const RomsetVerdict* nearest = nullptr;
    for (EmulatorOption& option : game.emulatorOptions) {
        const auto verdict = std::ranges::find(verdicts, option.id, &RomsetVerdict::emulatorId);
        if (verdict == verdicts.end()) {
            continue;
        }
        option.romset = verdict->fit;
        if (verdict->fit == RomsetFit::Mismatch) {
            option.romsetGap = gapOf(*verdict, set);
            if (verdict->listed &&
                (nearest == nullptr || verdict->missing.size() < nearest->missing.size())) {
                nearest = &*verdict;
            }
        }
    }
    std::vector<const EmulatorOption*> matching;
    for (const EmulatorOption& option : game.emulatorOptions) {
        if (option.romset == RomsetFit::Matches) {
            matching.push_back(&option);
        }
    }
    const auto use = [&game](const EmulatorOption& option) {
        game.emulator = option.name;
        game.launch = option.launch;
        game.unavailable = option.installed ? std::string{} : option.name + " is not installed";
    };
    const auto current =
        std::ranges::find(game.emulatorOptions, game.emulator, &EmulatorOption::name);
    if (!matching.empty()) {
        std::string names;
        for (const EmulatorOption* option : matching) {
            names += (names.empty() ? "" : ", ") + option->name;
        }
        game.romset = "Matches " + names;
        if (current != game.emulatorOptions.end() && current->romset == RomsetFit::Matches &&
            current->installed) {
            return;
        }
        const auto installed = std::ranges::find_if(matching, &EmulatorOption::installed);
        use(installed != matching.end() ? **installed : *matching.front());
        return;
    }
    const bool unchecked = std::ranges::any_of(game.emulatorOptions, [](const auto& option) {
        return !option.id.empty() && option.romset == RomsetFit::Unchecked;
    });
    const bool pending = std::ranges::any_of(game.emulatorOptions, [](const auto& option) {
        return option.romset == RomsetFit::Pending;
    });
    game.romset = std::string{pending ? checkingRomset : noRomsetMatch};
    if (nearest != nullptr) {
        const auto owner =
            std::ranges::find(game.emulatorOptions, nearest->emulatorId, &EmulatorOption::id);
        game.romsetNearest = owner->name + " " + gapOf(*nearest, set);
    }
    if (pending) {
        game.emulator.clear();
        game.launch = {};
        game.unavailable = std::string{romsetListsDownloading};
        return;
    }
    if (unchecked) {
        return;
    }
    game.emulator.clear();
    game.launch = {};
    game.unavailable = std::string{noRomsetMatch};
}

RomsetMatcher::RomsetMatcher(RomsetLists& lists) : lists_{&lists}, dats_{lists.dats()} {
}

fs::path RomsetMatcher::keptFile() const {
    return dats_.dir() / "matches.json";
}

void RomsetMatcher::begin(Clock::time_point now) {
    now_ = now;
    stamps_.clear();
    loaded_.clear();
    loadedFolder_.clear();
}

void RomsetMatcher::loadKept() {
    keptLoaded_ = true;
    std::ifstream in{keptFile(), std::ios::binary};
    if (!in) {
        return;
    }
    const json root = json::parse(in, nullptr, false);
    if (root.is_discarded() || !root.is_object() || root.value("version", 0) != keptVersion) {
        return;
    }
    try {
        for (const auto& [path, value] : root.at("zips").items()) {
            Kept kept;
            kept.zip = stampFrom(value.at("zip"));
            for (const auto& [id, stamp] : value.at("stamps").items()) {
                kept.stamps.emplace(id, stamp.get<std::uint32_t>());
            }
            for (const json& looked : value.at("looked")) {
                kept.looked.push_back(stampFrom(looked));
            }
            for (const json& verdict : value.at("verdicts")) {
                kept.verdicts.push_back(RomsetVerdict{
                    verdict.at("id").get<std::string>(),
                    fitOf(verdict.at("fit").get<std::string>()), verdict.at("listed").get<bool>(),
                    verdict.at("required").get<std::size_t>(),
                    verdict.at("missing").get<std::vector<std::string>>(),
                    verdict.at("zips").get<std::vector<std::string>>()});
            }
            kept_.emplace(path, std::move(kept));
        }
    } catch (const json::exception& malformed) {
        lucent::warn("roms", "{}: {}; romsets are matched again", keptFile().string(),
                     malformed.what());
        kept_.clear();
    }
}

std::map<std::string, std::uint32_t, std::less<>>
RomsetMatcher::stampsOf(std::span<const EmulatorEntry* const> cores) {
    std::map<std::string, std::uint32_t, std::less<>> stamps;
    for (const EmulatorEntry* entry : cores) {
        auto found = stamps_.find(entry->id);
        if (found == stamps_.end()) {
            lists_->want(*entry, now_);
            found = stamps_.emplace(entry->id, dats_.stamp(*entry)).first;
        }
        if (found->second) {
            stamps.emplace(entry->id, found->second->source);
        }
    }
    return stamps;
}

const RomsetDat* RomsetMatcher::datFor(const EmulatorEntry& entry, const RomFolder& folder) {
    if (folder.location() != loadedFolder_) {
        loaded_.clear();
        loadedFolder_ = folder.location();
    }
    auto found = loaded_.find(entry.id);
    if (found == loaded_.end()) {
        std::string error;
        std::optional<RomsetDat> dat = dats_.load(entry, folder.sets(), error);
        if (!dat) {
            lucent::warn("roms", "{}", error);
        }
        found = loaded_.emplace(entry.id, std::move(dat)).first;
    }
    return found->second ? &*found->second : nullptr;
}

std::vector<RomsetVerdict> RomsetMatcher::check(const RomFolder& folder, const RomNode& zip,
                                                std::span<const EmulatorEntry* const> cores) {
    std::vector<RomsetVerdict> verdicts;
    if (!dats_.enabled() || cores.empty()) {
        return verdicts;
    }
    if (!keptLoaded_) {
        loadKept();
    }
    const std::map<std::string, std::uint32_t, std::less<>> stamps = stampsOf(cores);
    const bool pending = std::ranges::any_of(cores, [&](const EmulatorEntry* entry) {
        return !stamps.contains(entry->id) && lists_->fetching(entry->id);
    });
    FolderZips zips{folder};
    const FileStamp stamp = zips.stampOf(zip.name);
    const std::string key = zip.location.string();
    if (const auto kept = kept_.find(key);
        !pending && kept != kept_.end() && kept->second.zip == stamp &&
        kept->second.stamps == stamps &&
        std::ranges::all_of(kept->second.looked, [&](const FileStamp& looked) {
            return zips.stampOf(looked.path) == looked;
        })) {
        return kept->second.verdicts;
    }
    const std::string set = romsetOf(zip.name);
    for (const EmulatorEntry* entry : cores) {
        RomsetVerdict verdict;
        if (const RomsetDat* dat = stamps.contains(entry->id) ? datFor(*entry, folder) : nullptr) {
            verdict = matchSet(set, *dat, zips);
        } else if (lists_->fetching(entry->id)) {
            verdict.fit = RomsetFit::Pending;
        }
        verdict.emulatorId = entry->id;
        lucent::info("roms", "romset {}: {} {}{}", set, entry->name, fitName(verdict.fit),
                     verdict.fit == RomsetFit::Mismatch ? " (" + gapOf(verdict, set) + ")"
                                                        : std::string{});
        verdicts.push_back(std::move(verdict));
    }
    // A pending verdict is not kept: a fetch that fails leaves every stamp as it was.
    if (!pending) {
        kept_.insert_or_assign(key, Kept{stamp, stamps, zips.looked(), verdicts});
        changed_ = true;
    }
    return verdicts;
}

void RomsetMatcher::save() {
    if (!changed_) {
        return;
    }
    json zips = json::object();
    for (const auto& [path, kept] : kept_) {
        json looked = json::array();
        for (const FileStamp& stamp : kept.looked) {
            looked.push_back(stampJson(stamp));
        }
        json verdicts = json::array();
        for (const RomsetVerdict& verdict : kept.verdicts) {
            verdicts.push_back({{"id", verdict.emulatorId},
                                {"fit", fitName(verdict.fit)},
                                {"listed", verdict.listed},
                                {"required", verdict.required},
                                {"missing", verdict.missing},
                                {"zips", verdict.zips}});
        }
        zips[path] = {{"zip", stampJson(kept.zip)},
                      {"stamps", kept.stamps},
                      {"looked", std::move(looked)},
                      {"verdicts", std::move(verdicts)}};
    }
    const json root{{"version", keptVersion}, {"zips", std::move(zips)}};
    std::string error;
    if (!fileio::writeWhole(keptFile(), root.dump(1), error)) {
        lucent::warn("roms", "romset matches not kept: {}", error);
        return;
    }
    changed_ = false;
}

} // namespace opensu::library::roms
