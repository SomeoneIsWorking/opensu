#include "libretro_cores.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "core_bios.hpp"
#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "zip/zip_archive.hpp"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr std::string_view libraryExtension = ".so";
constexpr std::string_view zipExtension = ".so.zip";
constexpr std::string_view recordExtension = ".json";
constexpr std::size_t dateLength = 10;
constexpr std::size_t crcDigits = 8;
/// The listing of every core is tens of kilobytes.
constexpr std::size_t maxIndexBytes = std::size_t{4} * 1024 * 1024;
constexpr long httpOk = 200;
/// Every core's info file zipped is a few hundred kilobytes.
constexpr std::size_t maxInfoArchiveBytes = std::size_t{16} * 1024 * 1024;
/// Current MAME's core is larger than the zip reader's usual cap.
constexpr std::uint32_t maxCoreBytes = 1024U * 1024U * 1024U;

std::string textOf(const json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

std::optional<std::uint32_t> crcOf(std::string_view hex) {
    std::uint32_t value = 0;
    const auto [end, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), value, 16);
    if (ec != std::errc{} || end != hex.data() + hex.size() || hex.empty() ||
        hex.size() > crcDigits) {
        return std::nullopt;
    }
    return value;
}

std::string hexOf(std::uint32_t crc) {
    return std::format("{:08x}", crc);
}

/// The words of `line` split at spaces and tabs.
std::vector<std::string_view> wordsOf(std::string_view line) {
    std::vector<std::string_view> words;
    std::size_t at = 0;
    while (at < line.size()) {
        const std::size_t start = line.find_first_not_of(" \t\r", at);
        if (start == std::string_view::npos) {
            break;
        }
        const std::size_t end = std::min(line.find_first_of(" \t\r", start), line.size());
        words.push_back(line.substr(start, end - start));
        at = end;
    }
    return words;
}

std::optional<std::string> readWhole(const fs::path& file, std::string& error) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        error = "cannot read " + file.string();
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void removeQuietly(const fs::path& file) {
    std::error_code ec;
    fs::remove(file, ec);
}

/// The one core in the zip `bytes`, CRC-checked by the zip reader.
std::optional<std::string> coreIn(std::string bytes, const std::string& core, std::string& error) {
    const std::optional<zip::Archive> archive = zip::Archive::open(std::move(bytes), error);
    if (!archive) {
        error = "the download is not a zip: " + error;
        return std::nullopt;
    }
    const std::string name = core + std::string{libraryExtension};
    if (archive->entries().size() != 1 || archive->entries().front().name != name) {
        error = "the zip does not hold just " + name;
        return std::nullopt;
    }
    std::optional<std::string> library = archive->read(name, error, maxCoreBytes);
    if (!library) {
        error = name + " cannot be taken out of the zip: " + error;
    }
    return library;
}

} // namespace

std::optional<CoreBuild> coreBuildIn(std::string_view index, std::string_view core,
                                     std::string_view base, std::string& error) {
    const std::string file = std::string{core} + std::string{zipExtension};
    std::size_t at = 0;
    while (at < index.size()) {
        const std::size_t end = std::min(index.find('\n', at), index.size());
        const std::vector<std::string_view> words = wordsOf(index.substr(at, end - at));
        at = end + 1;
        if (words.size() != 3 || words[2] != file) {
            continue;
        }
        const std::optional<std::uint32_t> crc = crcOf(words[1]);
        if (words[0].size() != dateLength || !crc) {
            error = "the buildbot lists " + file + " without a date and CRC-32";
            return std::nullopt;
        }
        return CoreBuild{std::string{core}, std::string{words[0]}, *crc, std::string{base} + file};
    }
    error = "the buildbot does not list " + file;
    return std::nullopt;
}

DownloadedCores DownloadedCores::under(const fs::path& dataDir) {
    return DownloadedCores{dataDir / "cores"};
}

fs::path DownloadedCores::file(std::string_view core) const {
    return root_ / (std::string{core} + std::string{libraryExtension});
}

fs::path DownloadedCores::info(std::string_view core) const {
    return root_ / (std::string{core} + std::string{coreInfoExtension});
}

fs::path DownloadedCores::record(std::string_view core) const {
    return root_ / (std::string{core} + std::string{recordExtension});
}

std::optional<DownloadedCore> DownloadedCores::find(std::string_view core) const {
    std::ifstream in{record(core), std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    const json root = json::parse(std::istreambuf_iterator<char>{in},
                                  std::istreambuf_iterator<char>{}, nullptr, false);
    const std::optional<std::uint32_t> crc =
        root.is_object() ? crcOf(textOf(root, "crc32")) : std::nullopt;
    if (!crc) {
        lucent::warn("emulators", "{}: not a core record; ignored", record(core).string());
        return std::nullopt;
    }
    std::error_code ec;
    if (!fs::is_regular_file(file(core), ec)) {
        lucent::warn("emulators", "{}: the recorded core is missing", record(core).string());
        return std::nullopt;
    }
    return DownloadedCore{std::string{core}, file(core), textOf(root, "date"), *crc,
                          textOf(root, "url")};
}

bool DownloadedCores::installInfo(std::string_view core, std::string_view text,
                                  std::string& error) const {
    return fileio::writeWhole(info(core), text, error);
}

bool DownloadedCores::install(const CoreBuild& build, std::string_view library,
                              std::string_view text, std::string& error) const {
    if (!fileio::writeWhole(file(build.core), library, error) ||
        !installInfo(build.core, text, error)) {
        return false;
    }
    const json body{{"date", build.date}, {"crc32", hexOf(build.crc32)}, {"url", build.url}};
    if (!fileio::writeWhole(record(build.core), body.dump(2) + "\n", error)) {
        return false;
    }
    lucent::info("emulators", "{} {} installed: {} (CRC-32 {})", build.core, build.date,
                 file(build.core).string(), hexOf(build.crc32));
    return true;
}

bool DownloadedCores::remove(std::string_view core, std::string& error) const {
    for (const fs::path& path : {record(core), info(core), file(core)}) {
        std::error_code ec;
        fs::remove(path, ec);
        if (ec) {
            error = "cannot remove " + path.string() + ": " + ec.message();
            return false;
        }
    }
    lucent::info("emulators", "{} removed from {}", core, root_.string());
    return true;
}

CoreSearch CoreSearch::standard(const fs::path& home, const fs::path& dataDir) {
    return CoreSearch{DownloadedCores::under(dataDir).root(),
                      {home / ".config" / "retroarch" / "cores", "/usr/lib64/libretro",
                       "/usr/lib/libretro", "/usr/lib/x86_64-linux-gnu/libretro"},
                      {home / ".config" / "retroarch" / "info", "/usr/share/libretro/info"}};
}

std::optional<std::string> CoreSearch::info(std::string_view core) const {
    std::vector<fs::path> files;
    if (!downloads.empty()) {
        files.push_back(DownloadedCores{downloads}.info(core));
    }
    for (const fs::path& folder : infoFolders) {
        files.push_back(folder / (std::string{core} + std::string{coreInfoExtension}));
    }
    for (const fs::path& file : files) {
        std::error_code ec;
        if (!fs::is_regular_file(file, ec)) {
            continue;
        }
        std::string error;
        if (std::optional<std::string> text = readWhole(file, error)) {
            return text;
        }
        lucent::warn("emulators", "{}", error);
    }
    return std::nullopt;
}

std::optional<FoundCore> CoreSearch::find(std::string_view core) const {
    if (!downloads.empty()) {
        if (std::optional<DownloadedCore> ours = DownloadedCores{downloads}.find(core)) {
            fs::path file = ours->file;
            return FoundCore{std::move(file), std::move(ours)};
        }
    }
    const std::string name = std::string{core} + std::string{libraryExtension};
    for (const fs::path& folder : folders) {
        std::error_code ec;
        if (fs::is_regular_file(folder / name, ec)) {
            return FoundCore{folder / name, std::nullopt};
        }
    }
    return std::nullopt;
}

std::string CoreDownload::refusal(const std::string& url) const {
    const std::string origin = net::originOf(base_);
    if (origin.empty() || !url.starts_with(origin)) {
        return url + " is not on the buildbot " + origin;
    }
    return {};
}

std::optional<std::string> CoreDownload::index(std::string& error) const {
    const net::WebClient::Guard guard{maxIndexBytes, [this](const std::string& url) {
                                          return refusal(url);
                                      }};
    std::optional<net::WebClient::Answer> answer =
        web_.request(base_ + std::string{coreIndexFile}, {}, guard, error);
    if (!answer) {
        error = "the buildbot's listing cannot be read: " + error;
        return std::nullopt;
    }
    if (answer->status != httpOk) {
        error = "the buildbot's listing cannot be read: HTTP " + std::to_string(answer->status);
        return std::nullopt;
    }
    return std::move(answer->body);
}

std::optional<CoreBuild> CoreDownload::latest(std::string_view core, std::string& error) const {
    const std::optional<std::string> listing = index(error);
    if (!listing) {
        return std::nullopt;
    }
    return coreBuildIn(*listing, core, base_, error);
}

std::optional<std::string> CoreDownload::infoArchive(std::string& error) const {
    const net::WebClient::Guard guard{maxInfoArchiveBytes, [this](const std::string& url) {
                                          return refusal(url);
                                      }};
    std::optional<net::WebClient::Answer> answer =
        web_.request(net::originOf(base_) + std::string{coreInfoArchive}, {}, guard, error);
    if (!answer) {
        error = "the buildbot's core info cannot be read: " + error;
        return std::nullopt;
    }
    if (answer->status != httpOk) {
        error = "the buildbot's core info cannot be read: HTTP " + std::to_string(answer->status);
        return std::nullopt;
    }
    return std::move(answer->body);
}

std::optional<DownloadedCore> CoreDownload::install(const CoreBuild& build,
                                                    const DownloadedCores& store,
                                                    const net::WebClient::Progress& progress,
                                                    std::string& error) const {
    std::error_code ec;
    fs::create_directories(store.root(), ec);
    if (ec) {
        error = "cannot make " + store.root().string() + ": " + ec.message();
        return std::nullopt;
    }
    std::optional<std::string> infoBytes = infoArchive(error);
    const std::optional<CoreInfoArchive> archive =
        infoBytes ? CoreInfoArchive::open(std::move(*infoBytes), error) : std::nullopt;
    const std::optional<std::string> info =
        archive ? archive->info(build.core, error) : std::nullopt;
    if (!info) {
        return std::nullopt;
    }
    const fs::path part = store.root() / (build.core + std::string{zipExtension} + ".part");
    lucent::info("emulators", "downloading {} {} from {}", build.core, build.date, build.url);
    const bool fetched = web_.download(build.url, part, progress, error, [this](const auto& url) {
        return refusal(url);
    });
    std::optional<std::string> bytes = fetched ? readWhole(part, error) : std::nullopt;
    removeQuietly(part);
    if (!fetched) {
        if (error != "cancelled") {
            error = "the download failed: " + error;
        }
        return std::nullopt;
    }
    if (!bytes) {
        return std::nullopt;
    }
    const std::optional<std::string> library = coreIn(std::move(*bytes), build.core, error);
    if (!library) {
        return std::nullopt;
    }
    // The buildbot lists the CRC-32 of the library inside the zip, not of the zip.
    if (const std::uint32_t seen = zip::crc32Of(*library); seen != build.crc32) {
        error = "the core's CRC-32 " + hexOf(seen) + " is not the buildbot's " + hexOf(build.crc32);
        lucent::warn("emulators", "{} {}: {}", build.core, build.date, error);
        return std::nullopt;
    }
    if (!store.install(build, *library, *info, error)) {
        return std::nullopt;
    }
    return store.find(build.core);
}

} // namespace opensu::library::roms
