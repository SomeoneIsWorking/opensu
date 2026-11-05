// share_target — a folder or WebDAV share (`SaveShare`) as a `SaveTarget` that several openSUs
// share: under `opensu-saves/`, each game's status and bundle sealed with ChaCha20-Poly1305
// (`<hash of the key>.status`, `.bundle`; the key's profile among it, so each profile keeps its
// own there and meets only its linked copies on other openSUs), the file's name authenticated with
// it so files cannot be swapped. The key is the scrypt of the player's passphrase with the share's
// salt
// (`opensu-saves/key.json`, made by the first openSU to use the share, with a sealed check that
// says whether a passphrase is the right one). Given saves are kept: the bundle first, then the
// status that names it.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "save_share.hpp"
#include "save_target.hpp"
#include "secrets/crypto.hpp"

namespace opensu::save_sync {

/// The folder a share's files go under.
inline constexpr std::string_view shareFolder = "opensu-saves";

class ShareTarget final : public SaveTarget {
  public:
    ShareTarget(std::string id, std::string name, std::unique_ptr<SaveShare> share,
                std::string passphrase);
    ~ShareTarget() override;
    ShareTarget(const ShareTarget&) = delete;
    ShareTarget& operator=(const ShareTarget&) = delete;
    ShareTarget(ShareTarget&&) = delete;
    ShareTarget& operator=(ShareTarget&&) = delete;

    [[nodiscard]] std::string id() const override {
        return id_;
    }
    [[nodiscard]] std::string name() const override {
        return name_;
    }
    [[nodiscard]] bool reachable() const override;
    [[nodiscard]] Held status(const library::saves::SaveKey& key) override;
    [[nodiscard]] std::optional<SaveBundle> fetch(const library::saves::SaveKey& key,
                                                  std::string& error) override;
    [[nodiscard]] Given give(const library::saves::SaveKey& key, const SaveBundle& bundle,
                             const SaveStatus& status, bool picked, std::string& error) override;
    void took(const library::saves::SaveKey& /*key*/, const SaveStatus& /*status*/) override {
    }

  private:
    /// The key, read or made on first use; null, with `error`, when the share cannot be read or
    /// the passphrase does not open it.
    [[nodiscard]] const secrets::crypto::Bytes* unlock(std::string& error);
    /// `name`'s file opened; nothing with `error` when it cannot be read or opened, and nothing
    /// with an empty `error` when it is not there.
    [[nodiscard]] std::optional<std::string> opened(const std::string& name, std::string& error);
    [[nodiscard]] bool sealed(const std::string& name, const std::string& plain,
                              std::string& error);

    std::string id_;
    std::string name_;
    std::unique_ptr<SaveShare> share_;
    std::string passphrase_;
    std::optional<secrets::crypto::Bytes> key_;
};

} // namespace opensu::save_sync
