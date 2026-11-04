// peer_book — who this openSU is on the LAN and which openSUs it paired with: its own peer id, made
// once and kept, and each paired peer's id and name in `lan.json` under the config dir; each
// peer's secret in the secret store (`secrets::SecretStore`: the keyring, else an owner-only
// file), never in the file and never in a log line. Safe to use from any thread.
#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "peer_crypto.hpp"
#include "secrets/secret_store.hpp"

namespace opensu::lan {

/// The file the book is kept in, under the config dir.
inline constexpr std::string_view peerBookFile = "lan.json";

/// A peer id is this many random bytes, written as hex.
inline constexpr std::size_t peerIdBytes = 16;

/// Whether `text` can be a peer id: `peerIdBytes` as lower-case hex.
[[nodiscard]] bool isPeerId(std::string_view text) noexcept;

/// The longest name an openSU goes by on the LAN, in bytes; DNS-SD's limit on an instance name.
inline constexpr std::size_t longestName = 63;

/// `name` as it can be shown and announced: control characters dropped, spaces trimmed, cut to
/// `longestName` at a character boundary; empty when nothing is left.
[[nodiscard]] std::string shownName(std::string_view name);

/// Another openSU this one paired with.
struct Peer {
    std::string id;
    std::string name;

    bool operator==(const Peer&) const = default;
};

class PeerBook {
  public:
    /// Reads `file`, making and keeping this openSU's id the first time; throws
    /// `std::runtime_error` when a new id cannot be kept.
    PeerBook(std::filesystem::path file, secrets::SecretStore secrets);
    ~PeerBook();
    PeerBook(const PeerBook&) = delete;
    PeerBook& operator=(const PeerBook&) = delete;
    PeerBook(PeerBook&&) = delete;
    PeerBook& operator=(PeerBook&&) = delete;

    [[nodiscard]] const std::string& id() const noexcept {
        return id_;
    }
    [[nodiscard]] std::vector<Peer> paired() const;
    [[nodiscard]] std::optional<Peer> find(std::string_view id) const;
    /// The secret of the paired peer `id`; nothing when it is not paired or its secret is gone.
    [[nodiscard]] std::optional<crypto::Bytes> secretOf(std::string_view id) const;

    /// Keeps `peer` as paired with `secret`, replacing an earlier pairing with it. False with
    /// `error` when the secret or the file could not be kept; the book is then as it was.
    [[nodiscard]] bool remember(const Peer& peer, const crypto::Bytes& secret, std::string& error);
    /// Forgets `id` and its secret. False with `error` when either could not be cleared.
    [[nodiscard]] bool forget(std::string_view id, std::string& error);

  private:
    [[nodiscard]] bool write(const std::vector<Peer>& peers, std::string& error) const;

    std::filesystem::path file_;
    secrets::SecretStore secrets_;
    std::string id_;
    mutable std::mutex mutex_;
    std::vector<Peer> peers_;
    /// Secrets read from the store, wiped when they go.
    mutable std::map<std::string, crypto::Bytes, std::less<>> known_;
};

} // namespace opensu::lan
