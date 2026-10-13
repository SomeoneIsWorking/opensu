#include "downloaded_emulators.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr std::string_view recordFile = "installed.json";

std::string textOf(const json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

std::string_view spelling(Verification verified) {
    return verified == Verification::Digest ? "digest" : "observed";
}

} // namespace

DownloadedEmulators DownloadedEmulators::under(const fs::path& dataDir) {
    return DownloadedEmulators{dataDir / "emulators"};
}

fs::path DownloadedEmulators::record(std::string_view id) const {
    return root_ / id / recordFile;
}

fs::path DownloadedEmulators::versionDir(std::string_view id, std::string_view assetId) const {
    return root_ / id / assetId;
}

std::optional<DownloadedEmulator> DownloadedEmulators::find(std::string_view id) const {
    std::ifstream in{record(id), std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    const json root = json::parse(std::istreambuf_iterator<char>{in},
                                  std::istreambuf_iterator<char>{}, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        lucent::warn("emulators", "{}: not a record; ignored", record(id).string());
        return std::nullopt;
    }
    DownloadedEmulator found;
    found.id = std::string{id};
    found.version = textOf(root, "version");
    found.assetId = textOf(root, "assetId");
    found.file = root_ / id / textOf(root, "file");
    found.sha256 = textOf(root, "sha256");
    const auto size = root.find("size");
    found.size = size != root.end() && size->is_number_unsigned() ? size->get<std::uint64_t>() : 0;
    found.verified =
        textOf(root, "verified") == "digest" ? Verification::Digest : Verification::Observed;
    found.url = textOf(root, "url");
    std::error_code ec;
    if (found.assetId.empty() || !fs::is_regular_file(found.file, ec)) {
        lucent::warn("emulators", "{}: the recorded AppImage is missing", record(id).string());
        return std::nullopt;
    }
    return found;
}

bool DownloadedEmulators::install(const DownloadedEmulator& entry, std::string& error) const {
    const fs::path dir = root_ / entry.id;
    std::error_code ec;
    const fs::path file = fs::relative(entry.file, dir, ec);
    if (ec || file.empty()) {
        error = entry.file.string() + " is not under " + dir.string();
        return false;
    }
    const json body{{"version", entry.version}, {"assetId", entry.assetId},
                    {"file", file.string()},    {"sha256", entry.sha256},
                    {"size", entry.size},       {"verified", spelling(entry.verified)},
                    {"url", entry.url}};
    if (!fileio::writeWhole(record(entry.id), body.dump(2) + "\n", error)) {
        return false;
    }
    for (const fs::directory_entry& child : fs::directory_iterator{dir, ec}) {
        if (child.is_directory(ec) && child.path().filename() != entry.assetId) {
            fs::remove_all(child.path(), ec);
            if (ec) {
                lucent::warn("emulators", "{}: the old version stays: {}", child.path().string(),
                             ec.message());
            }
        }
    }
    lucent::info("emulators", "{} {} installed: {} ({} bytes, sha256 {}, {})", entry.id,
                 entry.version, entry.file.string(), entry.size, entry.sha256,
                 spelling(entry.verified));
    return true;
}

bool DownloadedEmulators::remove(std::string_view id, std::string& error) const {
    const fs::path dir = root_ / id;
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (ec) {
        error = "cannot remove " + dir.string() + ": " + ec.message();
        return false;
    }
    lucent::info("emulators", "{} removed from {}", id, dir.string());
    return true;
}

} // namespace opensu::library::roms
