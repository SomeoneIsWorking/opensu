// A fake SteamGridDB API (v2) on a loopback server: grids (covers) by Steam app id or SteamGridDB
// game id, filtered by `dimensions` as the API does, title search at
// `/search/autocomplete/<term>`, each image served at `/img/<name>` with an id unique across games.
// Every API call must carry `Authorization: Bearer <key>`, else it answers 401 as SteamGridDB does;
// image fetches are recorded with whether they carried one.
#pragma once

#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "support/loopback_server.hpp"

namespace opensu::test {

class SteamGridDbServer {
  public:
    /// One request the server saw.
    struct Seen {
        std::string path;
        std::string query;
        bool authorized{false};
    };

    explicit SteamGridDbServer(std::string key)
        : key_{std::move(key)}, server_{[this](const lucent::http::Request& request) {
              return answer(request);
          }} {
    }

    /// The API root openSU is pointed at.
    [[nodiscard]] std::string api() const {
        return server_.base() + "/api/v2";
    }

    /// A cover: its file name, size and bytes.
    struct Image {
        std::string name;
        std::string bytes;
        int width{600};
        int height{900};
    };

    /// Holds `image` as a cover of Steam app `appId`.
    void putSteam(const std::string& appId, Image image) {
        put("/api/v2/grids/steam/" + appId, std::move(image));
    }
    /// Lists `title` as SteamGridDB game `gameId` and holds `image` as its cover.
    void putGame(const std::string& title, int gameId, Image image) {
        {
            const std::lock_guard lock{mutex_};
            titles_[title] = gameId;
        }
        put("/api/v2/grids/game/" + std::to_string(gameId), std::move(image));
    }
    /// Lists `title` as game `gameId` with no images.
    void putTitle(const std::string& title, int gameId) {
        const std::lock_guard lock{mutex_};
        titles_[title] = gameId;
    }

    [[nodiscard]] std::vector<Seen> seen() const {
        const std::lock_guard lock{mutex_};
        return seen_;
    }

  private:
    void put(const std::string& listing, Image image) {
        const std::lock_guard lock{mutex_};
        files_["/img/" + image.name] = std::move(image.bytes);
        images_[listing].push_back({std::move(image), ++lastId_});
    }

    static std::string decoded(std::string_view text) {
        std::string out;
        for (std::size_t at = 0; at < text.size(); ++at) {
            if (text[at] == '%' && at + 2 < text.size()) {
                out.push_back(
                    static_cast<char>(std::stoi(std::string{text.substr(at + 1, 2)}, nullptr, 16)));
                at += 2;
            } else {
                out.push_back(text[at]);
            }
        }
        return out;
    }

    static lucent::http::Response listing(const nlohmann::json& data) {
        return reply(200, nlohmann::json{{"success", true}, {"data", data}}.dump());
    }

    lucent::http::Response answer(const lucent::http::Request& request) {
        const std::lock_guard lock{mutex_};
        const std::string path{request.path()};
        const std::optional<std::string_view> authorization = request.header("Authorization");
        const bool authorized = authorization && *authorization == "Bearer " + key_;
        seen_.push_back({path, std::string{request.query()}, authorization.has_value()});
        if (path.starts_with("/img/")) {
            const auto found = files_.find(path);
            return found == files_.end()
                       ? reply(404, "")
                       : lucent::http::Response::binary(200, "OK", "image/png", found->second);
        }
        if (!authorized) {
            return reply(401, R"({"success":false,"errors":["Invalid API key"]})");
        }
        const std::string search = "/api/v2/search/autocomplete/";
        if (path.starts_with(search)) {
            nlohmann::json data = nlohmann::json::array();
            const auto found = titles_.find(decoded(path.substr(search.size())));
            if (found != titles_.end()) {
                data.push_back({{"id", found->second},
                                {"name", found->first},
                                {"types", {"steam"}},
                                {"verified", true}});
            }
            return listing(data);
        }
        const auto found = images_.find(path);
        if (found == images_.end()) {
            return reply(404, R"({"success":false,"errors":["Game not found"]})");
        }
        const std::string query{request.query()};
        const std::string filter = "dimensions=";
        const std::size_t at = query.find(filter);
        const std::string sizes =
            at == std::string::npos ? "" : decoded(query.substr(at + filter.size()));
        nlohmann::json data = nlohmann::json::array();
        for (const auto& [image, id] : found->second) {
            const std::string size =
                std::to_string(image.width) + "x" + std::to_string(image.height);
            if (!sizes.empty() && ("," + sizes + ",").find("," + size + ",") == std::string::npos) {
                continue;
            }
            data.push_back({{"id", id},
                            {"url", server_.base() + "/img/" + image.name},
                            {"thumb", server_.base() + "/img/" + image.name},
                            {"width", image.width},
                            {"height", image.height}});
        }
        return listing(data);
    }

    std::string key_;
    mutable std::mutex mutex_;
    /// Each listing's images with their ids, which run across every listing as SteamGridDB's do.
    std::map<std::string, std::vector<std::pair<Image, int>>> images_;
    int lastId_{0};
    std::map<std::string, std::string> files_;
    std::map<std::string, int> titles_;
    std::vector<Seen> seen_;
    LoopbackServer server_;
};

} // namespace opensu::test
