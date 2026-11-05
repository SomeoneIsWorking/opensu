#include "share_target.hpp"

#include <utility>

#include <nlohmann/json.hpp>

#include "json_text.hpp"
#include "secrets/secret.hpp"

namespace opensu::save_sync {
namespace {

using library::saves::SaveKey;
using nlohmann::json;

constexpr std::size_t saltBytes = 16;
/// How much of the key's hash names a game's files.
constexpr std::size_t nameBytes = 16;
constexpr std::string_view checkText = "opensu-saves";

std::string keyFile() {
    return std::string{shareFolder} + "/key.json";
}

std::string fileOf(const SaveKey& key, std::string_view kind) {
    return std::string{shareFolder} + "/" +
           secrets::hexText(
               secrets::crypto::sha256(key.profile + "\n" + key.system + "\n" + key.game))
               .substr(0, nameBytes * 2) +
           std::string{kind};
}

} // namespace

ShareTarget::ShareTarget(std::string id, std::string name, std::unique_ptr<SaveShare> share,
                         std::string passphrase)
    : id_{std::move(id)}, name_{std::move(name)}, share_{std::move(share)},
      passphrase_{std::move(passphrase)} {
}

ShareTarget::~ShareTarget() {
    secrets::wipe(passphrase_);
    if (key_) {
        secrets::wipeBytes(key_->data(), key_->size());
    }
}

bool ShareTarget::reachable() const {
    return share_->reachable();
}

const secrets::crypto::Bytes* ShareTarget::unlock(const std::stop_token& stop, std::string& error) {
    if (key_) {
        return &*key_;
    }
    if (passphrase_.empty()) {
        error = "no passphrase is set for " + name_;
        return nullptr;
    }
    const SaveShare::Read read = share_->read(stop, keyFile());
    if (read.kind == SaveShare::Read::Kind::Stopped) {
        error = "stopped";
        return nullptr;
    }
    if (read.kind == SaveShare::Read::Kind::Failed) {
        error = name_ + " cannot be read: " + read.bytes;
        return nullptr;
    }
    if (read.kind == SaveShare::Read::Kind::Missing) {
        const secrets::crypto::Bytes salt = secrets::crypto::randomBytes(saltBytes);
        const secrets::crypto::Bytes nonce =
            secrets::crypto::randomBytes(secrets::crypto::sealNonceBytes);
        secrets::crypto::Bytes key = secrets::crypto::scryptKey(passphrase_, salt);
        const std::string check = secrets::crypto::seal(key, nonce, keyFile(), checkText);
        const json document{
            {"salt", secrets::hexText(salt)},
            {"nonce", secrets::hexText(nonce)},
            {"check", secrets::hexText(std::span{
                          reinterpret_cast<const std::uint8_t*>(check.data()), check.size()})}};
        if (!share_->write(stop, keyFile(), document.dump(2) + "\n", error)) {
            secrets::wipeBytes(key.data(), key.size());
            return nullptr;
        }
        return &key_.emplace(std::move(key));
    }
    const json document = json::parse(read.bytes, nullptr, false);
    const std::optional<secrets::crypto::Bytes> salt =
        document.is_object() ? secrets::hexBytes(textField(document, "salt")) : std::nullopt;
    const std::optional<secrets::crypto::Bytes> nonce =
        document.is_object() ? secrets::hexBytes(textField(document, "nonce")) : std::nullopt;
    const std::optional<secrets::crypto::Bytes> check =
        document.is_object() ? secrets::hexBytes(textField(document, "check")) : std::nullopt;
    if (!salt || !nonce || !check || nonce->size() != secrets::crypto::sealNonceBytes) {
        error = keyFile() + " in " + name_ + " is not openSU's";
        return nullptr;
    }
    secrets::crypto::Bytes key = secrets::crypto::scryptKey(passphrase_, *salt);
    if (secrets::crypto::open(key, *nonce, keyFile(), secrets::crypto::asText(*check)) !=
        std::optional<std::string>{checkText}) {
        secrets::wipeBytes(key.data(), key.size());
        error = "the passphrase does not open the saves in " + name_;
        return nullptr;
    }
    return &key_.emplace(std::move(key));
}

std::optional<std::string> ShareTarget::opened(const std::stop_token& stop, const std::string& name,
                                               std::string& error) {
    error.clear();
    const secrets::crypto::Bytes* key = unlock(stop, error);
    if (key == nullptr) {
        return std::nullopt;
    }
    const SaveShare::Read read = share_->read(stop, name);
    if (read.kind == SaveShare::Read::Kind::Missing) {
        return std::nullopt;
    }
    if (read.kind == SaveShare::Read::Kind::Stopped) {
        error = "stopped";
        return std::nullopt;
    }
    if (read.kind == SaveShare::Read::Kind::Failed) {
        error = name + " cannot be read: " + read.bytes;
        return std::nullopt;
    }
    if (read.bytes.size() < secrets::crypto::sealNonceBytes) {
        error = name + " is not sealed by openSU";
        return std::nullopt;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(read.bytes.data());
    std::optional<std::string> plain =
        secrets::crypto::open(*key, std::span{bytes, secrets::crypto::sealNonceBytes}, name,
                              std::string_view{read.bytes}.substr(secrets::crypto::sealNonceBytes));
    if (!plain) {
        error = name + " does not open with this share's key";
    }
    return plain;
}

bool ShareTarget::sealed(const std::stop_token& stop, const std::string& name,
                         const std::string& plain, std::string& error) {
    const secrets::crypto::Bytes* key = unlock(stop, error);
    if (key == nullptr) {
        return false;
    }
    const secrets::crypto::Bytes nonce =
        secrets::crypto::randomBytes(secrets::crypto::sealNonceBytes);
    std::string bytes{secrets::crypto::asText(nonce)};
    bytes += secrets::crypto::seal(*key, nonce, name, plain);
    return share_->write(stop, name, bytes, error);
}

SaveTarget::Held ShareTarget::status(const std::stop_token& stop, const SaveKey& key) {
    std::string error;
    const std::optional<std::string> text = opened(stop, fileOf(key, ".status"), error);
    if (stop.stop_requested()) {
        return Held{.kind = Held::Kind::Stopped, .status = {}, .problem = {}};
    }
    if (!error.empty()) {
        return Held{.kind = Held::Kind::Failed, .status = {}, .problem = std::move(error)};
    }
    if (!text) {
        return Held{.kind = Held::Kind::Saves, .status = {}, .problem = {}};
    }
    std::optional<SaveStatus> status = readSaveStatus(*text);
    if (!status) {
        return Held{.kind = Held::Kind::Failed,
                    .status = {},
                    .problem = fileOf(key, ".status") + " is not a status of saves"};
    }
    return Held{.kind = Held::Kind::Saves, .status = std::move(*status), .problem = {}};
}

std::optional<SaveBundle> ShareTarget::fetch(const std::stop_token& stop, const SaveKey& key,
                                             std::string& error) {
    const std::optional<std::string> bytes = opened(stop, fileOf(key, ".bundle"), error);
    if (!bytes) {
        if (error.empty()) {
            error = "it holds no saves of the game";
        }
        return std::nullopt;
    }
    std::optional<SaveBundle> bundle = readBundle(*bytes);
    if (!bundle) {
        error = "what it holds is not a game's saves";
    }
    return bundle;
}

SaveTarget::Given ShareTarget::give(const std::stop_token& stop, const SaveKey& key,
                                    const SaveBundle& bundle, const SaveStatus& status,
                                    bool /*picked*/, std::string& error) {
    if (!sealed(stop, fileOf(key, ".bundle"), write(bundle), error) ||
        !sealed(stop, fileOf(key, ".status"), write(status), error)) {
        if (stop.stop_requested()) {
            return Given::Stopped;
        }
        error = name_ + " did not take the saves: " + error;
        return Given::Failed;
    }
    return Given::Kept;
}

} // namespace opensu::save_sync
