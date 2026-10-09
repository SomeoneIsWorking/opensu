#include "gog_installs.hpp"

#include <fstream>
#include <stdexcept>
#include <system_error>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"

namespace opensu::library::gog {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

} // namespace

Paths Paths::under(const fs::path& dataDir) {
    return Paths{.records = dataDir / "gog-installs.json",
                 .games = dataDir / "gog-games",
                 .prefixes = dataDir / "gog-prefixes",
                 .gogdlAuth = dataDir / "gogdl-auth.json"};
}

InstallRecords::InstallRecords(fs::path file) : file_{std::move(file)} {
}

std::map<std::string, Installed, std::less<>> InstallRecords::all() const {
    std::map<std::string, Installed, std::less<>> records;
    std::error_code ec;
    if (!fs::exists(file_, ec)) {
        return records;
    }
    std::ifstream in{file_};
    const json document = json::parse(in, nullptr, false);
    if (!document.is_object()) {
        throw std::runtime_error{file_.string() + " is not a list of GOG installs"};
    }
    for (const auto& [id, entry] : document.items()) {
        if (entry.is_object() && entry.contains("path") && entry.contains("platform")) {
            records[id] =
                Installed{.path = entry.value("path", ""), .platform = entry.value("platform", "")};
        }
    }
    return records;
}

std::optional<Installed> InstallRecords::find(std::string_view id) const {
    const auto records = all();
    const auto found = records.find(id);
    if (found == records.end()) {
        return std::nullopt;
    }
    return found->second;
}

void InstallRecords::add(const std::string& id, const Installed& installed) const {
    json document = json::object();
    for (const auto& [known, entry] : all()) {
        document[known] = {{"path", entry.path.string()}, {"platform", entry.platform}};
    }
    document[id] = {{"path", installed.path.string()}, {"platform", installed.platform}};
    std::string error;
    if (!fileio::writeWhole(file_, document.dump(2), error)) {
        throw std::runtime_error{error};
    }
}

} // namespace opensu::library::gog
