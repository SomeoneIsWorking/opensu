#include "gog.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library::gog {
namespace {

using nlohmann::json;

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
        // "worksOn": {"Windows": bool, "Mac": bool, "Linux": bool}
        const json worksOn = product.value("worksOn", json::object());
        game.builds.windows = worksOn.value("Windows", false);
        game.builds.linuxNative = worksOn.value("Linux", false);
        page.games.push_back(std::move(game));
    }
    return page;
}

} // namespace

Provider::Provider(TokenStore store, Setup setup)
    : auth_{std::move(store), setup.endpoints, web_}, setup_{std::move(setup)} {
}

void Provider::applyInstall(Game& game,
                            const std::map<std::string, Installed, std::less<>>& installs) const {
    const auto install = installs.find(game.sourceId);
    if (install == installs.end()) {
        return;
    }
    game.installed = true;
    game.launch = LaunchSpec{.program = setup_.gogdl,
                             .args = {"launch", install->second.path.string(), game.sourceId,
                                      "--platform", install->second.platform}};
    if (install->second.platform == "windows") {
        game.launch.args.insert(game.launch.args.end(),
                                {"--wine", setup_.wine, "--wine-prefix",
                                 (setup_.paths.prefixes / game.sourceId).string()});
    }
    // The install folder name identifies the running game.
    game.processHint = install->second.path.filename().string();
}

std::vector<Game> Provider::list() {
    const std::vector<std::string> headers{"Authorization: Bearer " + auth_.accessToken()};
    const std::map<std::string, Installed, std::less<>> installs =
        InstallRecords{setup_.paths.records}.all();
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
        for (Game& game : parsed.games) {
            applyInstall(game, installs);
        }
        std::ranges::move(parsed.games, std::back_inserter(games));
    }
    lucent::info("gog", "library lists {} games", games.size());
    return games;
}

} // namespace opensu::library::gog
