#include "wire.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "dns_sd.hpp"
#include "fileio/relative_path.hpp"
#include "peer_book.hpp"
#include "profiles/profile_book.hpp"
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
std::optional<secrets::crypto::Bytes> bytesField(const json& document, const char* key,
                                                 std::size_t size) {
    std::optional<secrets::crypto::Bytes> bytes = secrets::hexBytes(textField(document, key));
    if (!bytes || bytes->size() != size) {
        return std::nullopt;
    }
    return bytes;
}

/// A save key's part: a name a folder could be called, at most 255 bytes.
bool keyPart(std::string_view text) {
    constexpr std::size_t longest = 255;
    return !text.empty() && text.size() <= longest && text != "." && text != ".." &&
           std::ranges::none_of(text, [](char c) {
               return c == '/' || c == '\\' || static_cast<unsigned char>(c) < ' ';
           });
}

/// An unsigned number field; nothing when it is absent or not one.
std::optional<std::uint64_t> countField(const json& document, const char* key) {
    const auto found = document.find(key);
    if (found == document.end() || !found->is_number_unsigned()) {
        return std::nullopt;
    }
    return found->get<std::uint64_t>();
}

} // namespace

std::string textField(const json& document, const char* key) {
    const auto found = document.find(key);
    return found != document.end() && found->is_string() ? found->get<std::string>()
                                                         : std::string{};
}

std::string write(const Hello& hello) {
    return json{{"id", hello.id}, {"name", hello.name}, {"v", std::string{protocolVersion}}}.dump();
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
    std::optional<secrets::crypto::Bytes> commitment =
        bytesField(*document, "commitment", secrets::crypto::keyBytes);
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
    std::optional<secrets::crypto::Bytes> key =
        bytesField(*document, "key", secrets::crypto::keyBytes);
    std::optional<secrets::crypto::Bytes> nonce =
        bytesField(*document, "nonce", secrets::crypto::nonceBytes);
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
    std::optional<secrets::crypto::Bytes> key =
        bytesField(*document, "key", secrets::crypto::keyBytes);
    std::optional<secrets::crypto::Bytes> nonce =
        bytesField(*document, "nonce", secrets::crypto::nonceBytes);
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

namespace {

json profileEntry(const ListedProfile& profile) {
    return json{{"id", profile.id},
                {"name", profile.name},
                {"picture", profile.picture},
                {"linked", profile.linked}};
}

std::optional<ListedProfile> profileOf(const json& entry) {
    if (!entry.is_object()) {
        return std::nullopt;
    }
    ListedProfile profile{.id = textField(entry, "id"),
                          .name = shownName(textField(entry, "name")),
                          .picture = static_cast<std::uint8_t>(
                              countField(entry, "picture").value_or(0) % profiles::pictureCount),
                          .linked = entry.value("linked", false)};
    if (!profiles::isProfileId(profile.id) || profile.name.empty()) {
        return std::nullopt;
    }
    return profile;
}

} // namespace

std::string write(const std::vector<ListedProfile>& profiles) {
    json list = json::array();
    for (const ListedProfile& profile : profiles) {
        list.push_back(profileEntry(profile));
    }
    return json{{"profiles", list}}.dump();
}

std::optional<std::vector<ListedProfile>> readProfileListing(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document || !document->contains("profiles") || !(*document)["profiles"].is_array()) {
        return std::nullopt;
    }
    std::vector<ListedProfile> listed;
    for (const json& entry : (*document)["profiles"]) {
        if (std::optional<ListedProfile> profile = profileOf(entry)) {
            listed.push_back(std::move(*profile));
        }
    }
    return listed;
}

std::string write(const ProfileAsk& ask) {
    return json{{"profile", ask.profile}}.dump();
}

std::optional<ProfileAsk> readProfileAsk(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    ProfileAsk ask{textField(*document, "profile")};
    if (!profiles::isProfileId(ask.profile)) {
        return std::nullopt;
    }
    return ask;
}

std::string write(const CopyVerdict& verdict) {
    json document{{"verdict", verdict.verdict}};
    if (!verdict.profile.id.empty()) {
        document["profile"] = profileEntry(verdict.profile);
    }
    return document.dump();
}

std::optional<CopyVerdict> readCopyVerdict(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    CopyVerdict verdict{.verdict = textField(*document, "verdict"), .profile = {}};
    if (document->contains("profile")) {
        if (std::optional<ListedProfile> profile = profileOf((*document)["profile"])) {
            verdict.profile = std::move(*profile);
        }
    }
    if (verdict.verdict == "accepted" && verdict.profile.id.empty()) {
        return std::nullopt;
    }
    return verdict;
}

std::string write(const SaveAsk& ask) {
    return json{{"system", ask.system}, {"game", ask.game}, {"profile", ask.profile},
                {"picked", ask.picked}, {"hash", ask.hash}, {"counter", ask.counter}}
        .dump();
}

std::optional<SaveAsk> readSaveAsk(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    SaveAsk ask{.system = textField(*document, "system"),
                .game = textField(*document, "game"),
                .profile = textField(*document, "profile"),
                .picked = document->value("picked", false),
                .hash = textField(*document, "hash"),
                .counter = countField(*document, "counter").value_or(0)};
    if (!keyPart(ask.system) || !keyPart(ask.game) || !profiles::isProfileId(ask.profile)) {
        return std::nullopt;
    }
    return ask;
}

std::string write(const PlayRequest& request) {
    return json{{"game", request.game}, {"salt", secrets::hexText(request.salt)}}.dump();
}

std::optional<PlayRequest> readPlayRequest(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    std::optional<secrets::crypto::Bytes> salt =
        bytesField(*document, "salt", secrets::crypto::nonceBytes);
    std::string game = textField(*document, "game");
    if (!salt || game.empty()) {
        return std::nullopt;
    }
    return PlayRequest{std::move(game), std::move(*salt)};
}

std::string write(const PlayAnswer& answer) {
    return json{{"port", answer.port}, {"salt", secrets::hexText(answer.salt)}}.dump();
}

std::optional<PlayAnswer> readPlayAnswer(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> port = countField(*document, "port");
    std::optional<secrets::crypto::Bytes> salt =
        bytesField(*document, "salt", secrets::crypto::nonceBytes);
    if (!port || *port == 0 || *port > 0xffff || !salt) {
        return std::nullopt;
    }
    return PlayAnswer{static_cast<std::uint16_t>(*port), std::move(*salt)};
}

namespace {

json filesOf(const std::vector<RomFile>& files) {
    json list = json::array();
    for (const RomFile& file : files) {
        list.push_back(json{{"path", file.path}, {"bytes", file.bytes}});
    }
    return list;
}

/// The files of `document[key]`; nothing when one is malformed or could step out of its folder.
std::optional<std::vector<RomFile>> filesIn(const json& document, const char* key) {
    const auto found = document.find(key);
    if (found == document.end() || !found->is_array()) {
        return std::nullopt;
    }
    std::vector<RomFile> files;
    for (const json& entry : *found) {
        if (!entry.is_object()) {
            return std::nullopt;
        }
        RomFile file{textField(entry, "path"), countField(entry, "bytes").value_or(0)};
        if (file.path.empty() || !fileio::staysInside(file.path)) {
            return std::nullopt;
        }
        files.push_back(std::move(file));
    }
    return files;
}

constexpr std::string_view romKindKey(RomKind kind) noexcept {
    return kind == RomKind::Bios ? "bios" : "rom";
}

} // namespace

std::uint64_t RomGame::bytes() const {
    std::uint64_t total = 0;
    for (const RomFile& file : files) {
        total += file.bytes;
    }
    return total;
}

std::string write(const std::vector<RomConsole>& consoles) {
    json list = json::array();
    for (const RomConsole& console : consoles) {
        json games = json::array();
        for (const RomGame& game : console.games) {
            games.push_back(json{{"title", game.title},
                                 {"root", game.root},
                                 {"entry", game.entry},
                                 {"files", filesOf(game.files)}});
        }
        list.push_back(json{{"system", console.system},
                            {"folder", console.folder},
                            {"games", games},
                            {"bios", filesOf(console.bios)}});
    }
    return json{{"consoles", list}}.dump();
}

std::optional<std::vector<RomConsole>> readRomListing(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document || !document->contains("consoles") || !(*document)["consoles"].is_array()) {
        return std::nullopt;
    }
    std::vector<RomConsole> consoles;
    for (const json& entry : (*document)["consoles"]) {
        if (!entry.is_object()) {
            continue;
        }
        RomConsole console{textField(entry, "system"), textField(entry, "folder"), {}, {}};
        if (!keyPart(console.system) || !keyPart(console.folder)) {
            continue;
        }
        console.bios = filesIn(entry, "bios").value_or(std::vector<RomFile>{});
        const auto games = entry.find("games");
        if (games == entry.end() || !games->is_array()) {
            continue;
        }
        for (const json& listed : *games) {
            if (!listed.is_object()) {
                continue;
            }
            std::optional<std::vector<RomFile>> files = filesIn(listed, "files");
            const std::optional<std::uint64_t> root = countField(listed, "root");
            RomGame game{textField(listed, "title"), 0, textField(listed, "entry"), {}};
            const std::string inside = game.entry + "/";
            if (!files || files->empty() || !root || *root > 0xffffffffU || game.title.empty() ||
                game.entry.empty() || !fileio::staysInside(game.entry) ||
                !game.entry.starts_with(console.folder + "/")) {
                continue;
            }
            const bool contained = std::ranges::all_of(*files, [&](const RomFile& file) {
                return file.path == game.entry || file.path.starts_with(inside);
            });
            if (!contained) {
                continue;
            }
            game.root = static_cast<std::uint32_t>(*root);
            game.files = std::move(*files);
            console.games.push_back(std::move(game));
        }
        consoles.push_back(std::move(console));
    }
    return consoles;
}

std::string write(const RomAsk& ask) {
    return json{{"kind", std::string{romKindKey(ask.kind)}},
                {"root", ask.root},
                {"path", ask.path},
                {"offset", ask.offset},
                {"length", ask.length}}
        .dump();
}

std::optional<RomAsk> readRomAsk(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    const std::string kind = textField(*document, "kind");
    const std::optional<std::uint64_t> root = countField(*document, "root");
    RomAsk ask{kind == romKindKey(RomKind::Bios) ? RomKind::Bios : RomKind::Rom, 0,
               textField(*document, "path"), countField(*document, "offset").value_or(0),
               countField(*document, "length").value_or(0)};
    if ((kind != romKindKey(RomKind::Rom) && kind != romKindKey(RomKind::Bios)) || !root ||
        *root > 0xffffffffU || ask.path.empty() || !fileio::staysInside(ask.path)) {
        return std::nullopt;
    }
    ask.root = static_cast<std::uint32_t>(*root);
    return ask;
}

std::string write(const RomFacts& facts) {
    return json{{"bytes", facts.bytes}, {"sha256", facts.sha256}}.dump();
}

std::optional<RomFacts> readRomFacts(std::string_view body) {
    const std::optional<json> document = objectOf(body);
    if (!document) {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> bytes = countField(*document, "bytes");
    RomFacts facts{bytes.value_or(0), textField(*document, "sha256")};
    if (!bytes || (!facts.sha256.empty() &&
                   (facts.sha256.size() != 64 || !secrets::isLowerHex(facts.sha256)))) {
        return std::nullopt;
    }
    return facts;
}

} // namespace opensu::lan::wire
