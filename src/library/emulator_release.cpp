#include "emulator_release.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <system_error>

#include <nlohmann/json.hpp>

#include "lucent/content.h"
#include "lucent/log.h"

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

std::optional<ReleaseAsset> EmulatorRelease::latest(const ReleaseSource& source,
                                                    std::string& error) const {
    const std::optional<std::string> body =
        web_.get(source.api, error, {"Accept: application/json"});
    if (!body) {
        error = "the release listing cannot be read: " + error;
        return std::nullopt;
    }
    return releaseAssetOf(*body, source, error);
}

std::optional<DownloadedEmulator> EmulatorRelease::install(const EmulatorEntry& entry,
                                                           const ReleaseAsset& asset,
                                                           const DownloadedEmulators& store,
                                                           const net::WebClient::Progress& progress,
                                                           std::string& error) const {
    const fs::path dir = store.versionDir(entry.id, asset.assetId);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        error = "cannot make " + dir.string() + ": " + ec.message();
        return std::nullopt;
    }
    const fs::path part = dir / (asset.name + ".part");
    lucent::info("emulators", "downloading {} {} from {}", entry.name, asset.version, asset.url);
    if (!web_.download(asset.url, part, progress, error)) {
        removeQuietly(part);
        if (error != "cancelled") {
            error = "the download failed: " + error;
        }
        return std::nullopt;
    }
    const std::uint64_t size = fs::file_size(part, ec);
    if (ec || (asset.size != 0 && size != asset.size)) {
        removeQuietly(part);
        error = "the download is " + std::to_string(size) + " bytes, the release says " +
                std::to_string(asset.size);
        return std::nullopt;
    }
    const std::optional<lucent::content::Sha256> digest = lucent::content::sha256_file(part, error);
    if (!digest) {
        removeQuietly(part);
        return std::nullopt;
    }
    const std::string seen = lucent::content::sha256_hex(*digest);
    if (!asset.sha256.empty() && seen != asset.sha256) {
        removeQuietly(part);
        error = "the download's SHA-256 " + seen + " is not the release's " + asset.sha256;
        lucent::warn("emulators", "{} {}: {}", entry.name, asset.version, error);
        return std::nullopt;
    }
    const fs::path file = dir / asset.name;
    fs::permissions(part,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                        fs::perms::others_read | fs::perms::others_exec,
                    ec);
    if (!ec) {
        fs::rename(part, file, ec);
    }
    if (ec) {
        removeQuietly(part);
        error = "cannot put " + file.string() + " in place: " + ec.message();
        return std::nullopt;
    }
    DownloadedEmulator record{entry.id,
                              asset.version,
                              asset.assetId,
                              file,
                              seen,
                              size,
                              asset.sha256.empty() ? Verification::Observed : Verification::Digest,
                              asset.url};
    if (!store.install(record, error)) {
        return std::nullopt;
    }
    return record;
}

} // namespace opensu::library::roms
