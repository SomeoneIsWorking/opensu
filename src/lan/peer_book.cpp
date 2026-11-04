#include "peer_book.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "lucent/text.h"
#include "secrets/secret.hpp"
#include "wire.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

constexpr unsigned char deleteByte = 0x7F;
constexpr unsigned char continuationMask = 0xC0;
constexpr unsigned char continuationBits = 0x80;

std::string secretName(std::string_view id) {
    return "lan-peer-" + std::string{id};
}

} // namespace

bool isPeerId(std::string_view text) noexcept {
    return text.size() == peerIdBytes * 2 && secrets::isLowerHex(text);
}

std::string shownName(std::string_view name) {
    std::string kept;
    for (const char c : name) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte >= ' ' && byte != deleteByte) {
            kept.push_back(c);
        }
    }
    std::string out{lucent::text::trim(kept)};
    if (out.size() > longestName) {
        std::size_t end = longestName;
        while (end > 0 &&
               (static_cast<unsigned char>(out[end]) & continuationMask) == continuationBits) {
            --end;
        }
        out.resize(end);
        out = std::string{lucent::text::trim(out)};
    }
    return out;
}

PeerBook::PeerBook(fs::path file, secrets::SecretStore secrets)
    : file_{std::move(file)}, secrets_{std::move(secrets)} {
    std::error_code ec;
    if (fs::is_regular_file(file_, ec)) {
        std::ifstream in{file_};
        const json document = json::parse(in, nullptr, false);
        if (document.is_object()) {
            id_ = wire::textField(document, "id");
            if (const auto peers = document.find("peers");
                peers != document.end() && peers->is_array()) {
                for (const json& entry : *peers) {
                    if (!entry.is_object()) {
                        continue;
                    }
                    Peer peer{wire::textField(entry, "id"),
                              shownName(wire::textField(entry, "name")),
                              entry.value("shareSaves", false)};
                    if (isPeerId(peer.id) && peer.id != id_) {
                        peers_.push_back(std::move(peer));
                    }
                }
            }
        } else {
            lucent::warn("lan", "{} is not openSU's LAN book; starting a new one", file_.string());
        }
    }
    if (isPeerId(id_)) {
        return;
    }
    id_ = secrets::hexText(secrets::crypto::randomBytes(peerIdBytes));
    std::string error;
    if (!write(peers_, error)) {
        throw std::runtime_error{"this openSU's LAN id cannot be kept: " + error};
    }
}

PeerBook::~PeerBook() {
    for (auto& [id, secret] : known_) {
        secrets::wipeBytes(secret.data(), secret.size());
    }
}

std::vector<Peer> PeerBook::paired() const {
    const std::lock_guard lock{mutex_};
    return peers_;
}

std::optional<Peer> PeerBook::find(std::string_view id) const {
    const std::lock_guard lock{mutex_};
    const auto found = std::ranges::find(peers_, id, &Peer::id);
    if (found == peers_.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<secrets::crypto::Bytes> PeerBook::secretOf(std::string_view id) const {
    const std::lock_guard lock{mutex_};
    if (std::ranges::find(peers_, id, &Peer::id) == peers_.end()) {
        return std::nullopt;
    }
    if (const auto cached = known_.find(id); cached != known_.end()) {
        return cached->second;
    }
    std::string error;
    std::optional<secrets::SecretStore::Found> found = secrets_.read(secretName(id), error);
    if (!found) {
        if (!error.empty()) {
            lucent::warn("lan", "the secret of peer {} cannot be read: {}", id, error);
        }
        return std::nullopt;
    }
    std::optional<secrets::crypto::Bytes> secret = secrets::hexBytes(found->value);
    secrets::wipe(found->value);
    if (!secret || secret->size() != secrets::crypto::keyBytes) {
        lucent::warn("lan", "the secret kept for peer {} is damaged", id);
        return std::nullopt;
    }
    known_.emplace(std::string{id}, *secret);
    return secret;
}

bool PeerBook::remember(const Peer& peer, const secrets::crypto::Bytes& secret,
                        std::string& error) {
    const std::lock_guard lock{mutex_};
    std::string text = secrets::hexText(secret);
    const bool kept =
        secrets_.save(secretName(peer.id), "openSU LAN pairing with " + peer.name, text, error)
            .has_value();
    secrets::wipe(text);
    if (!kept) {
        return false;
    }
    std::vector<Peer> next = peers_;
    std::erase_if(next, [&peer](const Peer& known) {
        return known.id == peer.id;
    });
    next.push_back(peer);
    if (!write(next, error)) {
        return false;
    }
    peers_ = std::move(next);
    if (auto cached = known_.find(peer.id); cached != known_.end()) {
        secrets::wipeBytes(cached->second.data(), cached->second.size());
        known_.erase(cached);
    }
    known_.emplace(peer.id, secret);
    return true;
}

bool PeerBook::shareSaves(std::string_view id, bool on, std::string& error) {
    const std::lock_guard lock{mutex_};
    std::vector<Peer> next = peers_;
    const auto peer = std::ranges::find(next, id, &Peer::id);
    if (peer == next.end()) {
        error = "it is not paired with this openSU";
        return false;
    }
    peer->sharesSaves = on;
    if (!write(next, error)) {
        return false;
    }
    peers_ = std::move(next);
    return true;
}

bool PeerBook::forget(std::string_view id, std::string& error) {
    const std::lock_guard lock{mutex_};
    std::vector<Peer> next = peers_;
    std::erase_if(next, [id](const Peer& known) {
        return known.id == id;
    });
    if (!write(next, error)) {
        return false;
    }
    peers_ = std::move(next);
    if (auto cached = known_.find(id); cached != known_.end()) {
        secrets::wipeBytes(cached->second.data(), cached->second.size());
        known_.erase(cached);
    }
    return secrets_.forget(secretName(id), error);
}

bool PeerBook::write(const std::vector<Peer>& peers, std::string& error) const {
    json list = json::array();
    for (const Peer& peer : peers) {
        list.push_back({{"id", peer.id}, {"name", peer.name}, {"shareSaves", peer.sharesSaves}});
    }
    const json document{{"id", id_}, {"peers", list}};
    return fileio::writeWhole(file_, document.dump(2) + "\n", error);
}

} // namespace opensu::lan
