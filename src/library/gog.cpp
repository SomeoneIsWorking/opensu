#include "gog.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace iideck::library::gog {
namespace {

using nlohmann::json;

constexpr std::string_view installsLater = "GOG installs are not built yet";

/// One page of the account's games: `{"totalPages": N, "products": [{"id", "title", "image"}]}`.
struct Page {
    int totalPages{0};
    std::vector<Game> games;
};

/// GOG gives images as protocol-relative stems, `//images-N.gog.com/<hash>`, to which a size
/// like `_196.jpg` is appended.
std::string imageStem(const std::string& image) {
    return image.rfind("//", 0) == 0 ? "https:" + image : image;
}

/// The product id as text; GOG sends a number.
std::string productId(const json& product) {
    if (!product.contains("id")) {
        return {};
    }
    const json& id = product["id"];
    return id.is_string() ? id.get<std::string>() : id.dump();
}

Page parsePage(const std::string& body) {
    const json document = json::parse(body, nullptr, false);
    if (!document.is_object() || !document.contains("totalPages") ||
        !document.contains("products") || !document["products"].is_array()) {
        throw std::runtime_error{"GOG's library answer has an unexpected shape"};
    }
    Page page;
    page.totalPages = document.value("totalPages", 0);
    for (const json& product : document["products"]) {
        const std::string title = product.value("title", "");
        const std::string id = productId(product);
        if (title.empty() || id.empty()) {
            continue;
        }
        Game game;
        game.id = "gog:" + id;
        game.source = Source::Gog;
        game.sourceId = id;
        game.title = title;
        game.artworkUrl = imageStem(product.value("image", ""));
        game.unavailable = std::string{installsLater};
        page.games.push_back(std::move(game));
    }
    return page;
}

} // namespace

Provider::Provider(TokenStore store, Endpoints endpoints)
    : auth_{std::move(store), std::move(endpoints), web_} {
}

std::vector<Game> Provider::list() {
    const std::vector<std::string> headers{"Authorization: Bearer " + auth_.accessToken()};
    std::vector<Game> games;
    int totalPages = 1;
    for (int page = 1; page <= totalPages; ++page) {
        const std::string url =
            auth_.endpoints().embed +
            "/account/getFilteredProducts?mediaType=1&page=" + std::to_string(page);
        std::string error;
        const std::optional<std::string> body = web_.get(url, error, headers);
        if (!body) {
            throw std::runtime_error{"cannot read the GOG library (" + error + ")"};
        }
        Page parsed = parsePage(*body);
        totalPages = parsed.totalPages;
        std::ranges::move(parsed.games, std::back_inserter(games));
    }
    lucent::info("gog", "library lists {} games", games.size());
    return games;
}

} // namespace iideck::library::gog
