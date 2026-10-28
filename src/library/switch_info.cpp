#include "switch_info.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

#include "clock/file_time.hpp"
#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "rom_titles.hpp"
#include "switch_nca.hpp"
#include "switch_package.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

/// control.nacp's languages in its title table order, also its icons' names.
constexpr std::array<std::string_view, 16> languages{
    "AmericanEnglish", "BritishEnglish",       "Japanese",          "French",
    "German",          "LatinAmericanSpanish", "Spanish",           "Italian",
    "Dutch",           "CanadianFrench",       "Portuguese",        "Russian",
    "Korean",          "TraditionalChinese",   "SimplifiedChinese", "BrazilianPortuguese"};
constexpr std::size_t nacpTitleEntry = 0x300;
constexpr std::size_t nacpNameSize = 0x200;
constexpr std::uint64_t nacpSize = 0x4000;
/// A Switch icon is a 256x256 JPEG; a file far larger is not one.
constexpr std::uint64_t largestIcon = 0x80000;
/// A base title's id ends in 000; its update's in 800, add-ons count up from 001.
constexpr std::uint64_t titleIdVariant = 0xFFF;

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\n')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    return text;
}

/// The American English title, else the first a language gives.
std::string nacpTitle(std::string_view nacp) {
    if (nacp.size() < nacpSize) {
        return {};
    }
    for (std::size_t i = 0; i < languages.size(); ++i) {
        const std::string_view field = nacp.substr(i * nacpTitleEntry, nacpNameSize);
        const std::string_view name = trimmed(field.substr(0, field.find('\0')));
        if (!name.empty()) {
            return std::string{trimmed(withoutTrademarks(std::string{name}))};
        }
    }
    return {};
}

/// The American English icon, else the first language's there, else any icon.
const RomFsFile* iconOf(const std::vector<RomFsFile>& files) {
    for (const std::string_view language : languages) {
        const std::string name = "icon_" + std::string{language} + ".dat";
        if (const auto found = std::ranges::find(files, name, &RomFsFile::name);
            found != files.end()) {
            return &*found;
        }
    }
    const auto any = std::ranges::find_if(files, [](const RomFsFile& file) {
        return file.name.starts_with("icon_") && file.name.ends_with(".dat");
    });
    return any == files.end() ? nullptr : &*any;
}

bool isJpeg(std::string_view bytes) {
    return bytes.size() > 3 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
           static_cast<unsigned char>(bytes[1]) == 0xD8;
}

std::string readText(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

fs::path withSuffix(const fs::path& stem, std::string_view suffix) {
    fs::path out = stem;
    out += suffix;
    return out;
}

/// The cache name of `file` as it is now: its size and modification time.
std::optional<std::string> stampOf(const fs::path& file) {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(file, ec);
    if (ec) {
        return std::nullopt;
    }
    const fs::file_time_type written = fs::last_write_time(file, ec);
    if (ec) {
        return std::nullopt;
    }
    return std::to_string(size) + "-" + std::to_string(clock::sinceEpoch(written).count());
}

/// The Control NCA of the base title, else any Control NCA.
std::optional<Nca> controlNca(SwitchPackage& package, const Key256& headerKey, std::string& error) {
    std::optional<Nca> chosen;
    std::string firstError;
    for (const PackageFile& file : package.files()) {
        if (!file.name.ends_with(".nca") || file.name.ends_with(".cnmt.nca")) {
            continue;
        }
        std::string ncaError;
        std::optional<Nca> nca = Nca::open(package, file, headerKey, ncaError);
        if (!nca) {
            if (firstError.empty()) {
                firstError = ncaError;
            }
            continue;
        }
        if (nca->content() != NcaContent::Control) {
            continue;
        }
        const bool base = (nca->programId() & titleIdVariant) == 0;
        if (!chosen || base) {
            chosen = std::move(nca);
        }
        if (base) {
            break;
        }
    }
    if (!chosen) {
        error = firstError.empty() ? "no Control NCA in the package" : firstError;
    }
    return chosen;
}

} // namespace

SwitchInfo::SwitchInfo(std::vector<fs::path> keyDirs, fs::path cacheDir)
    : keyDirs_{std::move(keyDirs)}, cacheDir_{std::move(cacheDir)} {
}

std::vector<fs::path> SwitchInfo::edenKeyDirs(const fs::path& xdgDataHome, const fs::path& home) {
    return {xdgDataHome / "eden" / "keys", home / ".var/app/dev.eden_emu.eden/data/eden/keys"};
}

void SwitchInfo::begin() {
    keys_.reset();
    keysTried_ = false;
    keysError_.clear();
    fromCache_ = 0;
    extracted_ = 0;
    failed_ = 0;
    unkeyed_ = 0;
}

const SwitchKeys* SwitchInfo::keys() {
    if (!keysTried_) {
        keysTried_ = true;
        for (const fs::path& dir : keyDirs_) {
            std::string error;
            keys_ = SwitchKeys::load(dir, error);
            if (keys_) {
                lucent::info("switch", "keys read from {}", dir.string());
                break;
            }
            keysError_ += (keysError_.empty() ? "" : "; ") + error;
        }
    }
    return keys_ ? &*keys_ : nullptr;
}

std::optional<DumpInfo> SwitchInfo::read(const fs::path& file) {
    if (keyDirs_.empty() || cacheDir_.empty()) {
        return std::nullopt;
    }
    const std::optional<std::string> stamp = stampOf(file);
    if (!stamp) {
        ++failed_;
        lucent::warn("switch", "{}: cannot be read", file.string());
        return std::nullopt;
    }
    const fs::path stem = cacheDir_ / *stamp;
    std::error_code ec;
    if (const fs::path title = withSuffix(stem, ".title"); fs::is_regular_file(title, ec)) {
        DumpInfo info{.title = readText(title), .icon = {}};
        if (fs::path icon = withSuffix(stem, ".jpg"); fs::is_regular_file(icon, ec)) {
            info.icon = std::move(icon);
        }
        ++fromCache_;
        return info;
    }
    if (keys() == nullptr) {
        ++unkeyed_;
        return std::nullopt;
    }
    std::string error;
    std::optional<DumpInfo> info = extract(file, stem, error);
    if (!info) {
        ++failed_;
        lucent::warn("switch", "{}: {}", file.string(), error);
        return std::nullopt;
    }
    ++extracted_;
    return info;
}

std::optional<DumpInfo> SwitchInfo::extract(const fs::path& file, const fs::path& stem,
                                            std::string& error) {
    const SwitchKeys* keys = this->keys();
    const std::optional<Key256> headerKey = keys->headerKey();
    if (!headerKey) {
        error = "prod.keys lacks header_key and the keys it is made from";
        return std::nullopt;
    }
    std::optional<SwitchPackage> package = SwitchPackage::open(file, error);
    if (!package) {
        return std::nullopt;
    }
    std::optional<Nca> control = controlNca(*package, *headerKey, error);
    if (!control) {
        return std::nullopt;
    }
    std::vector<RomFsFile> files;
    if (!control->listRomFs(*keys, files, error)) {
        return std::nullopt;
    }
    const auto nacp = std::ranges::find(files, std::string{"control.nacp"}, &RomFsFile::name);
    const std::optional<std::string> nacpBytes =
        nacp == files.end() ? std::nullopt : control->readRomFs(*nacp, nacpSize);
    DumpInfo info{.title = nacpBytes ? nacpTitle(*nacpBytes) : std::string{}, .icon = {}};
    if (info.title.empty()) {
        error =
            nacp == files.end() ? "Control NCA has no control.nacp" : "control.nacp names no title";
        return std::nullopt;
    }
    if (const RomFsFile* icon = iconOf(files); icon != nullptr) {
        const std::optional<std::string> bytes = control->readRomFs(*icon, largestIcon);
        std::string writeError;
        if (!bytes || !isJpeg(*bytes)) {
            lucent::warn("switch", "{}: {} is not a JPEG", file.string(), icon->name);
        } else if (fs::path path = withSuffix(stem, ".jpg");
                   !fileio::writeWhole(path, *bytes, writeError)) {
            lucent::warn("switch", "{}", writeError);
        } else {
            info.icon = std::move(path);
        }
    }
    std::string writeError;
    if (!fileio::writeWhole(withSuffix(stem, ".title"), info.title, writeError)) {
        lucent::warn("switch", "{}", writeError);
    }
    return info;
}

void SwitchInfo::finish() const {
    const int total = fromCache_ + extracted_ + failed_ + unkeyed_;
    if (unkeyed_ > 0) {
        lucent::warn("switch", "{} Switch games keep their names: no Switch keys ({})", unkeyed_,
                     keysError_);
    }
    if (total == 0) {
        return;
    }
    lucent::info("switch",
                 "{} of {} Switch games titled from their own files ({} read now, {} "
                 "from the cache, {} failed)",
                 fromCache_ + extracted_, total, extracted_, fromCache_, failed_);
}

} // namespace opensu::library::roms
