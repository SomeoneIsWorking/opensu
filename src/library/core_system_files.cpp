#include "core_system_files.hpp"

#include <algorithm>
#include <set>
#include <system_error>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "fileio/read_whole.hpp"
#include "lucent/content.h"
#include "lucent/log.h"
#include "zip/zip_archive.hpp"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr std::string_view recordExtension = ".system.json";

void removeQuietly(const fs::path& file) {
    std::error_code ec;
    fs::remove(file, ec);
}

/// Removes the folders under `root` that `file`'s removal left empty.
void pruneUpTo(const fs::path& file, const fs::path& root) {
    std::error_code ec;
    for (fs::path folder = file.parent_path();
         folder != root && folder.native().starts_with(root.native());
         folder = folder.parent_path()) {
        if (!fs::is_empty(folder, ec) || ec || !fs::remove(folder, ec)) {
            return;
        }
    }
}

} // namespace

fs::path CoreSystemFiles::record(std::string_view core) const {
    return records_ / (std::string{core} + std::string{recordExtension});
}

std::optional<InstalledSystemFiles> CoreSystemFiles::installed(std::string_view core) const {
    std::string error;
    std::error_code ec;
    if (!fs::is_regular_file(record(core), ec)) {
        return std::nullopt;
    }
    const std::optional<std::string> text = fileio::readWhole(record(core), error);
    const json root = text ? json::parse(*text, nullptr, false) : json{};
    const auto field = [&root](const char* key) {
        const auto found = root.find(key);
        return found != root.end() && found->is_string() ? found->get<std::string>()
                                                         : std::string{};
    };
    if (!root.is_object() || !root.contains("files") || !root["files"].is_array()) {
        lucent::warn("emulators", "{}: not a system files record; ignored", record(core).string());
        return std::nullopt;
    }
    InstalledSystemFiles found{
        SystemFilesSource{field("url"), field("sha256"), field("folder"), field("into")}, {}};
    if (!isInsidePath(found.source.into)) {
        lucent::warn("emulators", "{}: its folder is not under the system folder; ignored",
                     record(core).string());
        return std::nullopt;
    }
    for (const json& file : root["files"]) {
        if (file.is_string() && isInsidePath(file.get<std::string>())) {
            found.files.push_back(file.get<std::string>());
        }
    }
    return found;
}

bool CoreSystemFiles::current(std::string_view core, const SystemFilesSource& source) const {
    const std::optional<InstalledSystemFiles> have = installed(core);
    return have && have->source == source;
}

bool CoreSystemFiles::writeRecord(std::string_view core, const InstalledSystemFiles& files,
                                  std::string& error) const {
    const json body{{"url", files.source.url},
                    {"sha256", files.source.sha256},
                    {"folder", files.source.folder},
                    {"into", files.source.into},
                    {"files", files.files}};
    return fileio::writeWhole(record(core), body.dump(1) + "\n", error);
}

net::Result<std::size_t> CoreSystemFiles::install(const std::stop_token& stop,
                                                  const net::WebClient& web, std::string_view core,
                                                  const SystemFilesSource& source,
                                                  const net::WebClient::Progress& progress) const {
    std::error_code ec;
    fs::create_directories(records_, ec);
    if (ec) {
        return net::Failure{"cannot make " + records_.string() + ": " + ec.message()};
    }
    const fs::path part = records_ / (std::string{core} + ".system.zip.part");
    const std::string origin = net::originOf(source.url);
    lucent::info("emulators", "downloading {}'s system files from {}", core, source.url);
    const net::Result<net::WebClient::BodyFacts> fetched =
        web.download(stop, source.url, part, progress, [&origin](const std::string& url) {
            return !origin.empty() && url.starts_with(origin)
                       ? std::string{}
                       : url + " is not on " + origin;
        });
    if (!fetched) {
        removeQuietly(part);
        return fetched.as<std::size_t>("the download failed: ");
    }
    std::string error;
    const std::optional<lucent::content::Sha256> digest =
        lucent::content::sha256_file(part, error);
    std::optional<std::string> bytes =
        digest ? fileio::readWhole(part, error) : std::optional<std::string>{};
    removeQuietly(part);
    if (!bytes) {
        return net::Failure{std::move(error)};
    }
    if (const std::string seen = lucent::content::sha256_hex(*digest); seen != source.sha256) {
        lucent::warn("emulators", "{}'s system files: SHA-256 {} is not the pinned {}", core, seen,
                     source.sha256);
        return net::Failure{"the system files' SHA-256 is not the one the catalog pins"};
    }
    const std::optional<zip::Archive> archive = zip::Archive::open(*std::move(bytes), error);
    if (!archive) {
        return net::Failure{"the system files are not a zip: " + error};
    }
    const std::string prefix = source.folder + "/";
    std::vector<std::string> wanted;
    for (const zip::Entry& entry : archive->entries()) {
        if (entry.name.starts_with(prefix) && !entry.name.ends_with('/')) {
            std::string relative = entry.name.substr(prefix.size());
            if (!isInsidePath(relative)) {
                return net::Failure{"the system files' zip names " + entry.name +
                                    ", outside its folder"};
            }
            wanted.push_back(std::move(relative));
        }
    }
    if (wanted.empty()) {
        return net::Failure{"the system files' zip holds nothing under " + source.folder};
    }

    const std::optional<InstalledSystemFiles> before = installed(core);
    std::set<std::string, std::less<>> ours;
    const fs::path oldRoot = before ? system_ / before->source.into : fs::path{};
    if (before && before->source.into == source.into) {
        ours.insert(before->files.begin(), before->files.end());
    }
    const fs::path root = system_ / source.into;
    std::vector<std::string> claimed;
    std::size_t left = 0;
    for (const std::string& relative : wanted) {
        if (ours.contains(relative) || !fs::exists(root / relative, ec)) {
            claimed.push_back(relative);
        } else {
            ++left;
        }
    }
    // Claimed before writing, so a write cut short is still openSU's to finish.
    InstalledSystemFiles pending{SystemFilesSource{}, claimed};
    pending.source.into = source.into;
    if (before) {
        for (const std::string& file : before->files) {
            if (before->source.into == source.into &&
                std::ranges::find(claimed, file) == claimed.end()) {
                pending.files.push_back(file);
            }
        }
    }
    if (!writeRecord(core, pending, error)) {
        return net::Failure{std::move(error)};
    }
    for (const std::string& relative : claimed) {
        if (stop.stop_requested()) {
            return net::Stopped{};
        }
        const std::string member = prefix + relative;
        const std::optional<std::string> file = archive->read(member, error);
        if (!file) {
            return net::Failure{member + " cannot be taken out of the zip: " + error};
        }
        if (!fileio::writeWhole(root / relative, *file, error)) {
            return net::Failure{std::move(error)};
        }
    }
    if (before) {
        const std::set<std::string, std::less<>> kept{claimed.begin(), claimed.end()};
        for (const std::string& file : before->files) {
            if (before->source.into != source.into || !kept.contains(file)) {
                removeQuietly(oldRoot / file);
                pruneUpTo(oldRoot / file, oldRoot);
            }
        }
    }
    if (!writeRecord(core, InstalledSystemFiles{source, claimed}, error)) {
        return net::Failure{std::move(error)};
    }
    lucent::info("emulators", "{}'s system files: {} written to {}, {} left as they were", core,
                 claimed.size(), root.string(), left);
    return claimed.size();
}

bool CoreSystemFiles::remove(std::string_view core, std::string& error) const {
    const std::optional<InstalledSystemFiles> have = installed(core);
    if (!have) {
        return true;
    }
    const fs::path root = system_ / have->source.into;
    for (const std::string& file : have->files) {
        std::error_code ec;
        fs::remove(root / file, ec);
        if (ec) {
            error = "cannot remove " + (root / file).string() + ": " + ec.message();
            return false;
        }
        pruneUpTo(root / file, root);
    }
    std::error_code ec;
    fs::remove(record(core), ec);
    if (ec) {
        error = "cannot remove " + record(core).string() + ": " + ec.message();
        return false;
    }
    lucent::info("emulators", "{}'s system files removed from {}", core, root.string());
    return true;
}

} // namespace opensu::library::roms
