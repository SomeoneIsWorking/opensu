#include "steamgriddb.hpp"

#include <nlohmann/json.hpp>

#include "libretro_index.hpp"
#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::artwork {
namespace {

using nlohmann::json;
using Status = SteamGridDb::Covers::Status;

/// An API answer is a small JSON listing.
constexpr std::size_t answerCap = 4U << 20U;
constexpr long httpOk = 200;
constexpr long httpUnauthorized = 401;
constexpr long httpForbidden = 403;
constexpr long httpNotFound = 404;

/// SteamGridDB's grid sizes of each shape.
std::string dimensionsOf(CoverShape shape) {
    switch (shape) {
    case CoverShape::Portrait:
        return "600x900,342x482,660x930";
    case CoverShape::Landscape:
        return "460x215,920x430";
    case CoverShape::Square:
        return "512x512,1024x1024";
    }
    return {};
}

bool isWebUrl(const std::string& url) {
    return url.starts_with("https://") || url.starts_with("http://");
}

} // namespace

SteamGridDbGame steamGridDbGameOf(const library::Game& game) {
    if (game.source == library::Source::Own) {
        return {game.artworkKey, game.title};
    }
    return {game.source == library::Source::Steam ? game.sourceId : "", game.title};
}

// A portrait is under 4:5, a landscape over 5:4.
CoverShape coverShapeOf(int width, int height) noexcept {
    if (width * 5 < height * 4) {
        return CoverShape::Portrait;
    }
    if (width * 4 > height * 5) {
        return CoverShape::Landscape;
    }
    return CoverShape::Square;
}

struct SteamGridDb::Data {
    Status status{Status::Listed};
    json data;
    std::string error;
};

SteamGridDb::SteamGridDb(std::string api) : api_{std::move(api)} {
}

void SteamGridDb::setKey(std::string key) {
    const std::lock_guard lock{mutex_};
    secrets::wipe(key_);
    key_ = std::move(key);
    refused_ = false;
}

bool SteamGridDb::refused() const {
    const std::lock_guard lock{mutex_};
    return refused_;
}

bool SteamGridDb::connected() const {
    const std::lock_guard lock{mutex_};
    return !key_.empty();
}

SteamGridDb::Data SteamGridDb::call(const net::WebClient& web, const std::string& path,
                                    const std::string& key) const {
    const std::string origin = net::originOf(api_);
    net::WebClient::Guard guard;
    guard.maxBody = answerCap;
    // The bearer key goes with every hop, so no hop may leave the API's origin.
    guard.refusal = [&origin](const std::string& url) {
        return net::originOf(url) == origin ? std::string{}
                                            : "SteamGridDB redirected off its API: " + url;
    };
    std::string header = "Authorization: Bearer " + key;
    std::string error;
    const std::optional<net::WebClient::Answer> answer =
        web.request(api_ + path, {header}, guard, error);
    secrets::wipe(header);
    if (!answer) {
        return {Status::Unreachable, {}, api_ + path + ": " + error};
    }
    if (answer->status == httpUnauthorized || answer->status == httpForbidden) {
        return {Status::Refused, {}, "SteamGridDB refused the key"};
    }
    if (answer->status == httpNotFound) {
        return {Status::Listed, nlohmann::json::array(), {}};
    }
    if (answer->status != httpOk) {
        return {Status::Unreachable, {}, api_ + path + ": HTTP " + std::to_string(answer->status)};
    }
    json document = json::parse(answer->body, nullptr, false);
    if (!document.is_object() || !document.value("success", false) || !document.contains("data")) {
        return {Status::Unreachable, {}, api_ + path + ": not a SteamGridDB answer"};
    }
    return {Status::Listed, std::move(document["data"]), {}};
}

SteamGridDb::Covers SteamGridDb::listed(const net::WebClient& web, const std::string& by,
                                        std::optional<CoverShape> shape,
                                        const std::string& key) const {
    const std::string query = shape ? "?dimensions=" + dimensionsOf(*shape) : "";
    const Data grids = call(web, "/grids" + by + query, key);
    Covers listed{grids.status, {}, grids.error};
    if (grids.status != Status::Listed || !grids.data.is_array()) {
        return listed;
    }
    for (const json& grid : grids.data) {
        if (!grid.is_object()) {
            continue;
        }
        SteamGridDbCover cover;
        cover.id = grid.value("id", std::int64_t{0});
        cover.url = grid.value("url", "");
        cover.thumb = grid.value("thumb", cover.url);
        cover.width = grid.value("width", 0);
        cover.height = grid.value("height", 0);
        if (isWebUrl(cover.url) && isWebUrl(cover.thumb)) {
            listed.covers.push_back(std::move(cover));
        }
    }
    return listed;
}

SteamGridDb::Covers SteamGridDb::covers(const net::WebClient& web, const SteamGridDbGame& game,
                                        std::optional<CoverShape> shape) const {
    std::string key;
    {
        const std::lock_guard lock{mutex_};
        key = key_;
    }
    if (key.empty()) {
        return {Status::Disconnected, {}, {}};
    }
    Covers found;
    if (!game.steamAppId.empty()) {
        found = listed(web, "/steam/" + encodeSegment(game.steamAppId), shape, key);
    }
    if (found.status == Status::Listed && found.covers.empty() && !game.title.empty()) {
        const Data matches = call(web, "/search/autocomplete/" + encodeSegment(game.title), key);
        if (matches.status != Status::Listed) {
            found = {matches.status, {}, matches.error};
        } else if (matches.data.is_array() && !matches.data.empty() &&
                   matches.data[0].is_object() && matches.data[0].contains("id") &&
                   matches.data[0]["id"].is_number_integer()) {
            const auto id = matches.data[0]["id"].get<std::int64_t>();
            found = listed(web, "/game/" + std::to_string(id), shape, key);
        }
    }
    secrets::wipe(key);
    if (found.status == Status::Refused) {
        const std::lock_guard lock{mutex_};
        if (!refused_) {
            lucent::warn("artwork", "{}", found.error);
        }
        refused_ = true;
    }
    return found;
}

} // namespace opensu::artwork
