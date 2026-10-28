// A fake libretro buildbot: the nightly folder's `.index-extended` and each core's zip under
// `/nightly/`, every published core's info file in `/assets/frontend/info.zip`, with one-shot
// redirects on request, on a loopback server.
#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "library/core_target.hpp"
#include "lucent/http.h"
#include "support/loopback_server.hpp"
#include "support/zip_fixture.hpp"

namespace opensu::test {

class BuildbotServer {
  public:
    BuildbotServer()
        : server_{[this](const lucent::http::Request& request) {
              return answer(request);
          }} {
    }

    /// Publishes `library` zipped as `core`'s build of `date`, and `info` as its info file. Like
    /// the real buildbot, the listing states the CRC-32 of the library inside the zip, or
    /// `listedCrc` when given.
    void publish(const std::string& core, const std::string& date, const std::string& library,
                 std::optional<std::uint32_t> listedCrc = std::nullopt,
                 const std::string& info = "firmware_count = \"0\"\n") {
        const std::string file = core + std::string{library::roms::coreTarget.librarySuffix};
        const std::string zip = zipfixture::buildZip({{file, library, true, {}}});
        const std::lock_guard lock{mutex_};
        infos_[core + ".info"] = info;
        files_["/nightly/" + file + ".zip"] = zip;
        listed_[core] =
            date + " " + hex(listedCrc.value_or(zipfixture::crcOf(library))) + " " + file + ".zip";
    }

    /// Answers the next request for `path` with a redirect to `target`.
    void redirect(const std::string& path, const std::string& target) {
        const std::lock_guard lock{mutex_};
        redirects_[path] = target;
    }

    /// The nightly folder, ending in /.
    [[nodiscard]] std::string base() const {
        return server_.base() + "/nightly/";
    }

    /// The same folder by another host name.
    [[nodiscard]] std::string otherHost() const {
        std::string other = base();
        other.replace(other.find("127.0.0.1"), std::string{"127.0.0.1"}.size(), "localhost");
        return other;
    }

  private:
    static std::string hex(std::uint32_t value) {
        std::array<char, 9> text{};
        std::snprintf(text.data(), text.size(), "%08x", value);
        return text.data();
    }

    lucent::http::Response answer(const lucent::http::Request& request) {
        const std::lock_guard lock{mutex_};
        const std::string path{request.path()};
        if (const auto moved = redirects_.find(path); moved != redirects_.end()) {
            lucent::http::Response response = reply(302, "");
            response.headers.push_back({"Location", moved->second});
            redirects_.erase(moved);
            return response;
        }
        if (path == "/nightly/.index-extended") {
            std::string index = "2026-01-01 0badf00d 2048_libretro" +
                                std::string{library::roms::coreTarget.librarySuffix} + ".zip\n";
            for (const auto& [core, line] : listed_) {
                index += line + "\n";
            }
            return lucent::http::Response::binary(200, "OK", "text/plain", index);
        }
        if (path == "/assets/frontend/info.zip") {
            std::vector<zipfixture::FixtureEntry> members;
            members.reserve(infos_.size());
            for (const auto& [name, text] : infos_) {
                members.push_back({name, text, true, {}});
            }
            return lucent::http::Response::binary(200, "OK", "application/zip",
                                                  zipfixture::buildZip(members));
        }
        const auto found = files_.find(path);
        if (found == files_.end()) {
            return reply(404, "");
        }
        return lucent::http::Response::binary(200, "OK", "application/zip", found->second);
    }

    std::mutex mutex_;
    std::map<std::string, std::string> files_;
    std::map<std::string, std::string> listed_;
    std::map<std::string, std::string> infos_;
    std::map<std::string, std::string> redirects_;
    LoopbackServer server_;
};

} // namespace opensu::test
