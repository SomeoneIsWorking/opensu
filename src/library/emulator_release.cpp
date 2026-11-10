#include "emulator_release.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <system_error>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/content.h"
#include "lucent/log.h"
#include "zip/zip_archive.hpp"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr std::string_view digestPrefix = "sha256:";
constexpr std::size_t dateLength = 10;

/// The string at `key`, or empty when it is absent, null or not a string: release APIs send null.
std::string textOf(const json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

std::uint64_t sizeOf(const json& object) {
    const auto found = object.find("size");
    return found != object.end() && found->is_number_unsigned() ? found->get<std::uint64_t>() : 0;
}

std::string idOf(const json& asset) {
    const auto id = asset.find("id");
    if (id == asset.end() || !(id->is_string() || id->is_number_integer())) {
        return {};
    }
    return id->is_string() ? id->get<std::string>() : id->dump();
}

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

/// A rolling tag ("latest", "continuous") names no version; its publish date does.
std::string versionOf(const json& release) {
    std::string tag = textOf(release, "tag_name");
    if (std::ranges::none_of(tag, [](unsigned char c) {
            return std::isdigit(c) != 0;
        })) {
        const std::string date = textOf(release, "published_at");
        if (date.size() >= dateLength) {
            tag += " " + date.substr(0, dateLength);
        }
    }
    return tag;
}

void removeQuietly(const fs::path& file) {
    std::error_code ec;
    fs::remove(file, ec);
}

std::optional<std::string> readWhole(const fs::path& file, std::string& error) {
    std::ifstream in{file, std::ios::binary};
    std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (!in && !in.eof()) {
        error = "cannot read " + file.string();
        return std::nullopt;
    }
    return bytes;
}

/// Takes the one AppImage matching `member` out of the verified zip `archive` into `dir`, as
/// `<name>.part`; the zip is removed either way. The part file, or nothing with `error`.
std::optional<fs::path> unpackMember(const fs::path& archive, const std::string& member,
                                     const fs::path& dir, std::string& error) {
    std::optional<std::string> bytes = readWhole(archive, error);
    removeQuietly(archive);
    if (!bytes) {
        return std::nullopt;
    }
    const std::optional<zip::Archive> zip = zip::Archive::open(std::move(*bytes), error);
    if (!zip) {
        return std::nullopt;
    }
    const std::regex pattern{member};
    const zip::Entry* chosen = nullptr;
    for (const zip::Entry& entry : zip->entries()) {
        if (!std::regex_match(entry.name, pattern)) {
            continue;
        }
        if (chosen != nullptr) {
            error = "the archive holds two files matching " + member;
            return std::nullopt;
        }
        chosen = &entry;
    }
    if (chosen == nullptr || chosen->name.find('/') != std::string::npos) {
        error = "the archive holds no file matching " + member;
        return std::nullopt;
    }
    const std::optional<std::string> file = zip->read(chosen->name, error);
    if (!file) {
        return std::nullopt;
    }
    fs::path part = dir / (chosen->name + ".part");
    if (!fileio::writeWhole(part, *file, error)) {
        removeQuietly(part);
        return std::nullopt;
    }
    return part;
}

} // namespace

std::optional<ReleaseAsset> releaseAssetOf(std::string_view body, const ReleaseSource& source,
                                           std::string& error) {
    const json release = json::parse(body, nullptr, false);
    if (release.is_discarded() || !release.is_object() || !release.contains("assets") ||
        !release["assets"].is_array()) {
        error = "the release listing is not a release";
        return std::nullopt;
    }
    const std::regex pattern{source.asset};
    const json* chosen = nullptr;
    for (const json& asset : release["assets"]) {
        if (!asset.is_object() || !std::regex_match(textOf(asset, "name"), pattern)) {
            continue;
        }
        if (chosen != nullptr) {
            error = "the release has two files matching " + source.asset;
            return std::nullopt;
        }
        chosen = &asset;
    }
    if (chosen == nullptr) {
        error = "the latest release has no file matching " + source.asset;
        return std::nullopt;
    }
    ReleaseAsset asset;
    asset.version = versionOf(release);
    asset.assetId = idOf(*chosen);
    asset.name = textOf(*chosen, "name");
    asset.url = textOf(*chosen, "browser_download_url");
    asset.size = sizeOf(*chosen);
    if (const std::string digest = textOf(*chosen, "digest"); digest.starts_with(digestPrefix)) {
        asset.sha256 = lowered(digest.substr(digestPrefix.size()));
    }
    if (!source.sha256.empty()) {
        if (!asset.sha256.empty() && asset.sha256 != source.sha256) {
            error = "the release's " + asset.name + " is not the one openSU pins: its SHA-256 is " +
                    asset.sha256 + ", not " + source.sha256;
            return std::nullopt;
        }
        asset.sha256 = source.sha256;
    }
    if (asset.assetId.empty() || asset.name.find('/') != std::string::npos) {
        error = "the release lists " + asset.name + " without an id";
        return std::nullopt;
    }
    if (!asset.url.starts_with(source.downloads)) {
        error = asset.url + " is not on the project's own host " + source.downloads;
        return std::nullopt;
    }
    return asset;
}

net::Result<ReleaseAsset> EmulatorRelease::latest(const std::stop_token& stop,
                                                  const ReleaseSource& source) const {
    const net::Result<std::string> body = web_.get(stop, source.api, {"Accept: application/json"});
    if (!body) {
        return body.as<ReleaseAsset>("the release listing cannot be read: ");
    }
    std::string error;
    std::optional<ReleaseAsset> asset = releaseAssetOf(*body, source, error);
    if (!asset) {
        return net::Failure{std::move(error)};
    }
    return std::move(*asset);
}

net::Result<std::string> EmulatorRelease::fetch(const std::stop_token& stop,
                                                const ReleaseAsset& asset, const fs::path& file,
                                                const net::WebClient::Progress& progress,
                                                const net::WebClient::Refusal& refusal) const {
    lucent::info("emulators", "downloading {} {} from {}", asset.name, asset.version, asset.url);
    if (const net::Result<net::WebClient::BodyFacts> fetched =
            web_.download(stop, asset.url, file, progress, refusal);
        !fetched) {
        removeQuietly(file);
        return fetched.as<std::string>("the download failed: ");
    }
    std::error_code ec;
    const std::uint64_t size = fs::file_size(file, ec);
    if (ec || (asset.size != 0 && size != asset.size)) {
        removeQuietly(file);
        return net::Failure{"the download is " + std::to_string(size) +
                            " bytes, the release says " + std::to_string(asset.size)};
    }
    std::string error;
    const std::optional<lucent::content::Sha256> digest = lucent::content::sha256_file(file, error);
    if (!digest) {
        removeQuietly(file);
        return net::Failure{std::move(error)};
    }
    std::string seen = lucent::content::sha256_hex(*digest);
    if (!asset.sha256.empty() && seen != asset.sha256) {
        removeQuietly(file);
        error = "the download's SHA-256 " + seen + " is not the release's " + asset.sha256;
        lucent::warn("emulators", "{} {}: {}", asset.name, asset.version, error);
        return net::Failure{std::move(error)};
    }
    return seen;
}

net::Result<DownloadedEmulator>
EmulatorRelease::install(const std::stop_token& stop, const EmulatorEntry& entry,
                         const ReleaseAsset& asset, const DownloadedEmulators& store,
                         const net::WebClient::Progress& progress) const {
    std::string error;
    const fs::path dir = store.versionDir(entry.id, asset.assetId);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        return net::Failure{"cannot make " + dir.string() + ": " + ec.message()};
    }
    const fs::path download = dir / (asset.name + ".part");
    const net::Result<std::string> seen = fetch(stop, asset, download, progress);
    if (!seen) {
        return seen.as<DownloadedEmulator>();
    }
    const std::uint64_t size = fs::file_size(download, ec);
    fs::path part = download;
    if (entry.release && !entry.release->member.empty()) {
        std::optional<fs::path> unpacked =
            unpackMember(download, entry.release->member, dir, error);
        if (!unpacked) {
            return net::Failure{asset.name + ": " + error};
        }
        part = std::move(*unpacked);
    }
    const fs::path file = part.parent_path() / part.stem();
    fs::permissions(part,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                        fs::perms::others_read | fs::perms::others_exec,
                    ec);
    if (!ec) {
        fs::rename(part, file, ec);
    }
    if (ec) {
        removeQuietly(part);
        return net::Failure{"cannot put " + file.string() + " in place: " + ec.message()};
    }
    DownloadedEmulator record{entry.id,
                              asset.version,
                              asset.assetId,
                              file,
                              *seen,
                              size,
                              asset.sha256.empty() ? Verification::Observed : Verification::Digest,
                              asset.url};
    if (!store.install(record, error)) {
        return net::Failure{std::move(error)};
    }
    return record;
}

} // namespace opensu::library::roms
