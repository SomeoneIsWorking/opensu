// A fake release host: an emulator's latest release as GitHub's release JSON at `/api/<id>`, and
// its assets at `/dl/<asset id>/<name>`, on a loopback server.
#pragma once

#include <map>
#include <mutex>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "library/emulator_catalog.hpp"
#include "lucent/content.h"
#include "support/loopback_server.hpp"

namespace opensu::test {

class ReleaseServer {
  public:
    ReleaseServer()
        : server_{[this](const lucent::http::Request& request) {
              return answer(request);
          }} {
    }

    /// Publishes `body` as the latest release of `id`, tagged `tag`, as asset `assetId` named
    /// `name`. The digest is the body's own, or `digest` when given ("" publishes none).
    void publish(const std::string& id, const std::string& tag, int assetId,
                 const std::string& name, const std::string& body,
                 const std::optional<std::string>& digest = std::nullopt) {
        const std::string own = lucent::content::sha256_hex(
            lucent::content::sha256(std::as_bytes(std::span{body.data(), body.size()})));
        const std::string published = digest.value_or(own);
        nlohmann::json asset{
            {"id", assetId},
            {"name", name},
            {"size", body.size()},
            {"browser_download_url", base() + "/dl/" + std::to_string(assetId) + "/" + name}};
        if (!published.empty()) {
            asset["digest"] = "sha256:" + published;
        }
        const nlohmann::json release{{"tag_name", tag},
                                     {"published_at", "2026-10-01T00:00:00Z"},
                                     {"assets", nlohmann::json::array({asset})}};
        const std::lock_guard lock{mutex_};
        files_["/api/" + id] = release.dump();
        files_["/dl/" + std::to_string(assetId) + "/" + name] = body;
    }

    /// Serves `body` at `path`.
    void put(const std::string& path, std::string body) {
        const std::lock_guard lock{mutex_};
        files_[path] = std::move(body);
    }

    /// What a test names of an emulator; the rest of its catalog entry points here.
    struct Emulator {
        std::string id;
        std::string name;
        std::string system;
        std::string asset;
    };

    /// The catalog entry of `emulator` with its releases here.
    [[nodiscard]] library::roms::EmulatorEntry entry(const Emulator& emulator) const {
        library::roms::EmulatorEntry entry;
        entry.id = emulator.id;
        entry.name = emulator.name;
        entry.systems = {emulator.system};
        entry.launchers = {
            library::roms::EmulatorLauncher{{emulator.id + "-not-on-path"}, "", "", {"{rom}"}}};
        entry.release = library::roms::ReleaseSource{base() + "/api/" + emulator.id, emulator.asset,
                                                     base() + "/dl/"};
        return entry;
    }

    [[nodiscard]] std::string base() const {
        return server_.base();
    }

  private:
    lucent::http::Response answer(const lucent::http::Request& request) {
        const std::lock_guard lock{mutex_};
        const auto found = files_.find(std::string{request.path()});
        if (found == files_.end()) {
            return reply(404, "");
        }
        return lucent::http::Response::binary(200, "OK", "application/octet-stream", found->second);
    }

    std::mutex mutex_;
    std::map<std::string, std::string> files_;
    LoopbackServer server_;
};

} // namespace opensu::test
