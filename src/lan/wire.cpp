#include "wire.hpp"

#include <nlohmann/json.hpp>

#include "peer_book.hpp"
#include "secrets/secret.hpp"

namespace opensu::lan::wire {
namespace {

using nlohmann::json;

/// `body` as a JSON object; nothing when it is not one.
std::optional<json> objectOf(std::string_view body) {
    json document = json::parse(body, nullptr, false);
    if (!document.is_object()) {
        return std::nullopt;
    }
    return document;
}

/// A hex field of exactly `size` bytes.
std::optional<crypto::Bytes> bytesField(const json& document, const char* key, std::size_t size) {
    std::optional<crypto::Bytes> bytes = secrets::hexBytes(textField(document, key));
    if (!bytes || bytes->size() != size) {
        return std::nullopt;
    }
    return bytes;
}

} // namespace

std::string textField(const json& document, const char* key) {
    const auto found = document.find(key);
    return found != document.end() && found->is_string() ? found->get<std::string>()
                                                         : std::string{};
}

std::string write(const Hello& hello) {
    return json{{"id", hello.id}, {"name", hello.name}, {"v", 1}}.dump();
}

std::optional<Hello> readHello(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    Hello hello{textField(*document, "id"), shownName(textField(*document, "name"))};
    if (!isPeerId(hello.id)) {
        return std::nullopt;
    }
    return hello;
}

std::string write(const PairRequest& request) {
    return json{{"id", request.id},
                {"name", request.name},
                {"commitment", secrets::hexText(request.commitment)}}
        .dump();
}

std::optional<PairRequest> readPairRequest(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    std::optional<crypto::Bytes> commitment = bytesField(*document, "commitment", crypto::keyBytes);
    PairRequest request{textField(*document, "id"), shownName(textField(*document, "name")), {}};
    if (!commitment || !isPeerId(request.id) || request.name.empty()) {
        return std::nullopt;
    }
    request.commitment = std::move(*commitment);
    return request;
}

std::string write(const PairOffer& offer) {
    return json{{"key", secrets::hexText(offer.key)}, {"nonce", secrets::hexText(offer.nonce)}}
        .dump();
}

std::optional<PairOffer> readPairOffer(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    std::optional<crypto::Bytes> key = bytesField(*document, "key", crypto::keyBytes);
    std::optional<crypto::Bytes> nonce = bytesField(*document, "nonce", crypto::nonceBytes);
    if (!key || !nonce) {
        return std::nullopt;
    }
    return PairOffer{std::move(*key), std::move(*nonce)};
}

std::string write(const PairReveal& reveal) {
    return json{{"id", reveal.id},
                {"key", secrets::hexText(reveal.key)},
                {"nonce", secrets::hexText(reveal.nonce)}}
        .dump();
}

std::optional<PairReveal> readPairReveal(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    std::optional<crypto::Bytes> key = bytesField(*document, "key", crypto::keyBytes);
    std::optional<crypto::Bytes> nonce = bytesField(*document, "nonce", crypto::nonceBytes);
    std::string id = textField(*document, "id");
    if (!key || !nonce || !isPeerId(id)) {
        return std::nullopt;
    }
    return PairReveal{std::move(id), std::move(*key), std::move(*nonce)};
}

std::string write(const PairPoll& poll) {
    return json{{"id", poll.id}}.dump();
}

std::optional<PairPoll> readPairPoll(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    PairPoll poll{textField(*document, "id")};
    if (!isPeerId(poll.id)) {
        return std::nullopt;
    }
    return poll;
}

std::string write(const PairVerdict& verdict) {
    return json{{"verdict", verdict.verdict}, {"signature", verdict.signature}}.dump();
}

std::optional<PairVerdict> readPairVerdict(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    PairVerdict verdict{textField(*document, "verdict"), textField(*document, "signature")};
    if (verdict.verdict.empty() || verdict.signature.empty()) {
        return std::nullopt;
    }
    return verdict;
}

bool isCoverToken(std::string_view text) noexcept {
    return text.size() == coverTokenBytes * 2 && secrets::isLowerHex(text);
}

std::string write(const std::vector<ListedGame>& games) {
    json list = json::array();
    for (const ListedGame& game : games) {
        json entry{{"id", game.id}, {"title", game.title}};
        if (!game.cover.empty()) {
            entry["cover"] = game.cover;
        }
        list.push_back(std::move(entry));
    }
    return json{{"games", list}}.dump();
}

std::optional<std::vector<ListedGame>> readListing(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document || !document->contains("games") || !(*document)["games"].is_array()) {
        return std::nullopt;
    }
    std::vector<ListedGame> games;
    for (const json& entry : (*document)["games"]) {
        if (!entry.is_object()) {
            continue;
        }
        ListedGame game{textField(entry, "id"), textField(entry, "title"),
                        textField(entry, "cover")};
        if (game.id.empty() || game.title.empty()) {
            continue;
        }
        if (!isCoverToken(game.cover)) {
            game.cover.clear();
        }
        games.push_back(std::move(game));
    }
    return games;
}

} // namespace opensu::lan::wire
