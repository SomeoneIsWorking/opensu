#include "shader_pack.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "zip/zip_archive.hpp"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr std::string_view packFolder = "slang";
constexpr std::string_view recordFile = "slang.json";
constexpr std::string_view partSuffix = ".part";
constexpr std::string_view oldSuffix = ".old";

std::optional<std::string> readWhole(const fs::path& file, std::string& error) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        error = "cannot read " + file.string();
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// `name` as a path inside the pack, or nothing when it would land outside it.
std::optional<fs::path> insidePack(std::string_view name) {
    fs::path path = fs::path{name}.lexically_normal();
    if (path.empty() || path.is_absolute() || *path.begin() == "..") {
        return std::nullopt;
    }
    return path;
}

void removeAll(const fs::path& folder) {
    std::error_code ec;
    fs::remove_all(folder, ec);
    if (ec) {
        lucent::warn("shaders", "cannot remove {}: {}", folder.string(), ec.message());
    }
}

/// Every file of `archive` written under `folder`, each checked against its CRC-32.
bool unpack(const zip::Archive& archive, const fs::path& folder, std::string& error) {
    std::size_t files = 0;
    for (const zip::Entry& entry : archive.entries()) {
        if (entry.name.ends_with('/')) {
            continue;
        }
        const std::optional<fs::path> path = insidePack(entry.name);
        if (!path) {
            error = "the pack holds " + entry.name + ", which lies outside it";
            return false;
        }
        const std::optional<std::string> bytes = archive.read(entry.name, error);
        if (!bytes) {
            error = entry.name + " cannot be taken out of the zip: " + std::move(error);
            return false;
        }
        if (!fileio::writeWhole(folder / *path, *bytes, error)) {
            return false;
        }
        ++files;
    }
    if (files == 0) {
        error = "the zip holds no files";
        return false;
    }
    lucent::info("shaders", "unpacked {} files into {}", files, folder.string());
    return true;
}

} // namespace

std::string ShaderPackBuild::version() const {
    const std::chrono::sys_seconds at{std::chrono::seconds{modified}};
    return std::format("{:%Y-%m-%d %H:%M}", at);
}

std::string ShaderPackBuild::key() const {
    return std::to_string(modified) + ":" + std::to_string(size);
}

std::string presetName(std::string_view path) {
    std::string_view name = path;
    if (const std::size_t slash = name.rfind('/'); slash != std::string_view::npos) {
        name.remove_prefix(slash + 1);
    }
    if (name.ends_with(presetExtension)) {
        name.remove_suffix(presetExtension.size());
    }
    return std::string{name};
}

ShaderPack ShaderPack::under(const fs::path& dataDir) {
    return ShaderPack{dataDir / "shaders"};
}

fs::path ShaderPack::root() const {
    return folder_ / packFolder;
}

fs::path ShaderPack::record() const {
    return folder_ / recordFile;
}

std::optional<ShaderPackBuild> ShaderPack::installed() const {
    std::ifstream in{record(), std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    const json root = json::parse(std::istreambuf_iterator<char>{in},
                                  std::istreambuf_iterator<char>{}, nullptr, false);
    if (!root.is_object() || !root.contains("modified") || !root["modified"].is_number_integer() ||
        !root.contains("size") || !root["size"].is_number_unsigned()) {
        lucent::warn("shaders", "{}: not a shader pack record; ignored", record().string());
        return std::nullopt;
    }
    std::error_code ec;
    if (!fs::is_directory(this->root(), ec)) {
        lucent::warn("shaders", "{}: the recorded pack is missing", record().string());
        return std::nullopt;
    }
    return ShaderPackBuild{root["modified"].get<std::int64_t>(), root["size"].get<std::uint64_t>(),
                           root.value("url", std::string{})};
}

bool ShaderPack::install(const ShaderPackBuild& build, std::string bytes,
                         std::string& error) const {
    const std::optional<zip::Archive> archive = zip::Archive::open(std::move(bytes), error);
    if (!archive) {
        error = "the download is not a zip: " + error;
        return false;
    }
    const fs::path part = folder_ / (std::string{packFolder} + std::string{partSuffix});
    const fs::path old = folder_ / (std::string{packFolder} + std::string{oldSuffix});
    removeAll(part);
    if (!unpack(*archive, part, error)) {
        removeAll(part);
        return false;
    }
    removeAll(old);
    std::error_code ec;
    if (fs::exists(root(), ec)) {
        fs::rename(root(), old, ec);
        if (ec) {
            error = "cannot move " + root().string() + " aside: " + ec.message();
            removeAll(part);
            return false;
        }
    }
    fs::rename(part, root(), ec);
    if (ec) {
        error = "cannot put the pack in " + root().string() + ": " + ec.message();
        fs::rename(old, root(), ec);
        removeAll(part);
        return false;
    }
    removeAll(old);
    const json body{{"modified", build.modified}, {"size", build.size}, {"url", build.url}};
    if (!fileio::writeWhole(record(), body.dump(2) + "\n", error)) {
        return false;
    }
    lucent::info("shaders", "shader pack {} installed in {}", build.version(), root().string());
    return true;
}

bool ShaderPack::remove(std::string& error) const {
    std::error_code ec;
    fs::remove(record(), ec);
    if (!ec) {
        fs::remove_all(root(), ec);
    }
    if (ec) {
        error = "cannot remove " + root().string() + ": " + ec.message();
        return false;
    }
    lucent::info("shaders", "shader pack removed from {}", root().string());
    return true;
}

std::vector<ShaderPreset> ShaderPack::presets() const {
    std::vector<ShaderPreset> found;
    std::error_code ec;
    for (fs::recursive_directory_iterator at{root(), ec}, end; !ec && at != end; at.increment(ec)) {
        if (!at->is_regular_file(ec) || at->path().extension() != presetExtension) {
            continue;
        }
        const std::string path = at->path().lexically_relative(root()).generic_string();
        const std::size_t slash = path.rfind('/');
        found.push_back(
            ShaderPreset{slash == std::string::npos ? std::string{} : path.substr(0, slash), path,
                         presetName(path)});
    }
    if (ec) {
        lucent::warn("shaders", "cannot list {}: {}", root().string(), ec.message());
    }
    std::ranges::sort(found, [](const ShaderPreset& left, const ShaderPreset& right) {
        return std::tie(left.folder, left.path) < std::tie(right.folder, right.path);
    });
    return found;
}

std::string ShaderPackDownload::refusal(const std::string& url) const {
    const std::string origin = net::originOf(url_);
    if (origin.empty() || !url.starts_with(origin)) {
        return url + " is not on the buildbot " + origin;
    }
    return {};
}

std::optional<ShaderPackBuild> ShaderPackDownload::latest(std::string& error) const {
    const std::optional<net::WebClient::BodyFacts> facts = web_.head(
        url_,
        [this](const std::string& url) {
            return refusal(url);
        },
        error);
    if (!facts) {
        error = "the buildbot's shader pack cannot be read: " + error;
        return std::nullopt;
    }
    if (facts->modified < 0 || facts->length <= 0) {
        error = "the buildbot serves the shader pack without a date and size";
        return std::nullopt;
    }
    return ShaderPackBuild{facts->modified, static_cast<std::uint64_t>(facts->length), url_};
}

std::optional<ShaderPackBuild> ShaderPackDownload::install(const ShaderPack& pack,
                                                           const net::WebClient::Progress& progress,
                                                           std::string& error) const {
    const fs::path folder = pack.root().parent_path();
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec) {
        error = "cannot make " + folder.string() + ": " + ec.message();
        return std::nullopt;
    }
    const fs::path part = folder / "shaders_slang.zip.part";
    lucent::info("shaders", "downloading the shader pack from {}", url_);
    net::WebClient::BodyFacts facts;
    const bool fetched = web_.download(
        url_, part, progress, error,
        [this](const auto& url) {
            return refusal(url);
        },
        &facts);
    std::optional<std::string> bytes = fetched ? readWhole(part, error) : std::nullopt;
    fs::remove(part, ec);
    if (!fetched) {
        if (error != "cancelled") {
            error = "the download failed: " + error;
        }
        return std::nullopt;
    }
    if (!bytes) {
        return std::nullopt;
    }
    // The buildbot publishes no digest: the length the response announced is what is checked.
    if (facts.length < 0 || static_cast<std::uint64_t>(facts.length) != bytes->size()) {
        error = "the pack is " + std::to_string(bytes->size()) + " bytes, not the " +
                std::to_string(facts.length) + " the buildbot announced";
        return std::nullopt;
    }
    const ShaderPackBuild build{std::max<std::int64_t>(facts.modified, 0), bytes->size(), url_};
    if (!pack.install(build, std::move(*bytes), error)) {
        return std::nullopt;
    }
    return pack.installed();
}

} // namespace opensu::library::roms
