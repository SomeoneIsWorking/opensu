#include "profile_book.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"
#include "save_sync/json_text.hpp"
#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::profiles {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;
using save_sync::textField;

std::string newId() {
    return secrets::hexText(secrets::crypto::randomBytes(profileIdBytes));
}

std::uint8_t pictureOf(const json& entry) {
    const auto found = entry.find("picture");
    if (found == entry.end() || !found->is_number_unsigned()) {
        return 0;
    }
    return static_cast<std::uint8_t>(found->get<unsigned>() % pictureCount);
}

} // namespace

std::string_view pictureName(std::uint8_t picture) noexcept {
    constexpr std::array<std::string_view, pictureCount> names{"Red",  "Orange", "Yellow", "Green",
                                                               "Teal", "Blue",   "Purple", "Pink"};
    return names[picture % pictureCount];
}

bool isProfileId(std::string_view text) noexcept {
    return text.size() == profileIdBytes * 2 && secrets::isLowerHex(text);
}

bool Profile::linkedWith(std::string_view peer) const {
    return std::ranges::find(links, peer) != links.end();
}

ProfileBook::ProfileBook(fs::path file) : file_{std::move(file)} {
    std::error_code ec;
    if (fs::is_regular_file(file_, ec)) {
        std::ifstream in{file_};
        const json document = json::parse(in, nullptr, false);
        if (document.is_object() && document.contains("profiles") &&
            document["profiles"].is_array()) {
            for (const json& entry : document["profiles"]) {
                if (!entry.is_object()) {
                    continue;
                }
                Profile profile{.id = textField(entry, "id"),
                                .name = textField(entry, "name"),
                                .picture = pictureOf(entry),
                                .links = {}};
                if (const auto links = entry.find("links");
                    links != entry.end() && links->is_array()) {
                    for (const json& peer : *links) {
                        if (peer.is_string()) {
                            profile.links.push_back(peer.get<std::string>());
                        }
                    }
                }
                if (isProfileId(profile.id) && at(profile.id) == nullptr) {
                    kept_.profiles.push_back(std::move(profile));
                }
            }
            kept_.active = textField(document, "active");
        } else {
            lucent::warn("profiles", "{} is not openSU's profiles; starting again", file_.string());
        }
    }
    if (!kept_.profiles.empty()) {
        if (at(kept_.active) == nullptr) {
            kept_.active = kept_.profiles.front().id;
        }
        return;
    }
    Kept made{.profiles = {Profile{
                  .id = newId(), .name = std::string{firstProfileName}, .picture = 0, .links = {}}},
              .active = {}};
    made.active = made.profiles.front().id;
    std::string error;
    if (!keep(std::move(made), error)) {
        throw std::runtime_error{"the first profile cannot be kept: " + error};
    }
    lucent::info("profiles", "made the first profile, {}", kept_.active);
}

std::vector<Profile> ProfileBook::all() const {
    const std::lock_guard lock{mutex_};
    return kept_.profiles;
}

std::optional<Profile> ProfileBook::find(std::string_view id) const {
    const std::lock_guard lock{mutex_};
    const Profile* found = at(id);
    return found != nullptr ? std::optional<Profile>{*found} : std::nullopt;
}

const Profile* ProfileBook::at(std::string_view id) const {
    const auto found = std::ranges::find(kept_.profiles, id, &Profile::id);
    return found == kept_.profiles.end() ? nullptr : &*found;
}

Profile ProfileBook::first() const {
    const std::lock_guard lock{mutex_};
    return kept_.profiles.front();
}

Profile ProfileBook::active() const {
    const std::lock_guard lock{mutex_};
    return *std::ranges::find(kept_.profiles, kept_.active, &Profile::id);
}

std::optional<std::size_t> ProfileBook::seatOf(std::string_view id) const {
    const std::lock_guard lock{mutex_};
    const auto found = std::ranges::find(kept_.profiles, id, &Profile::id);
    if (found == kept_.profiles.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - kept_.profiles.begin());
}

std::optional<Profile> ProfileBook::linked(std::string_view id, std::string_view peer) const {
    const std::lock_guard lock{mutex_};
    const Profile* found = at(id);
    if (found == nullptr || !found->linkedWith(peer)) {
        return std::nullopt;
    }
    return *found;
}

bool ProfileBook::activate(std::string_view id, std::string& error) {
    const std::lock_guard lock{mutex_};
    if (at(id) == nullptr) {
        error = "there is no such profile";
        return false;
    }
    Kept next = kept_;
    next.active = std::string{id};
    return keep(std::move(next), error);
}

std::optional<Profile> ProfileBook::add(std::string name, std::string& error) {
    const std::lock_guard lock{mutex_};
    Kept next = kept_;
    Profile made{.id = newId(),
                 .name = std::move(name),
                 .picture = static_cast<std::uint8_t>(next.profiles.size() % pictureCount),
                 .links = {}};
    next.profiles.push_back(made);
    if (!keep(std::move(next), error)) {
        return std::nullopt;
    }
    return made;
}

bool ProfileBook::rename(std::string_view id, std::string name, std::string& error) {
    const std::lock_guard lock{mutex_};
    Kept next = kept_;
    const auto found = std::ranges::find(next.profiles, id, &Profile::id);
    if (found == next.profiles.end()) {
        error = "there is no such profile";
        return false;
    }
    found->name = std::move(name);
    return keep(std::move(next), error);
}

bool ProfileBook::setPicture(std::string_view id, std::uint8_t picture, std::string& error) {
    const std::lock_guard lock{mutex_};
    Kept next = kept_;
    const auto found = std::ranges::find(next.profiles, id, &Profile::id);
    if (found == next.profiles.end()) {
        error = "there is no such profile";
        return false;
    }
    found->picture = static_cast<std::uint8_t>(picture % pictureCount);
    return keep(std::move(next), error);
}

bool ProfileBook::link(std::string_view id, std::string_view peer, bool on, std::string& error) {
    const std::lock_guard lock{mutex_};
    Kept next = kept_;
    const auto found = std::ranges::find(next.profiles, id, &Profile::id);
    if (found == next.profiles.end()) {
        error = "there is no such profile";
        return false;
    }
    std::erase(found->links, peer);
    if (on) {
        found->links.emplace_back(peer);
    }
    return keep(std::move(next), error);
}

bool ProfileBook::adopt(const Profile& copy, std::string_view peer, std::string& error) {
    if (!isProfileId(copy.id)) {
        error = "that is not a profile id";
        return false;
    }
    const std::lock_guard lock{mutex_};
    Kept next = kept_;
    auto found = std::ranges::find(next.profiles, copy.id, &Profile::id);
    if (found == next.profiles.end()) {
        next.profiles.push_back(
            Profile{.id = copy.id,
                    .name = copy.name,
                    .picture = static_cast<std::uint8_t>(copy.picture % pictureCount),
                    .links = {}});
        found = std::prev(next.profiles.end());
    }
    std::erase(found->links, peer);
    found->links.emplace_back(peer);
    return keep(std::move(next), error);
}

bool ProfileBook::forgetPeer(std::string_view peer, std::string& error) {
    const std::lock_guard lock{mutex_};
    Kept next = kept_;
    bool changed = false;
    for (Profile& profile : next.profiles) {
        changed = std::erase(profile.links, peer) > 0 || changed;
    }
    return !changed || keep(std::move(next), error);
}

bool ProfileBook::keep(Kept next, std::string& error) {
    json list = json::array();
    for (const Profile& profile : next.profiles) {
        list.push_back({{"id", profile.id},
                        {"name", profile.name},
                        {"picture", profile.picture},
                        {"links", profile.links}});
    }
    const json document{{"active", next.active}, {"profiles", std::move(list)}};
    if (!fileio::writeWhole(file_, document.dump(2) + "\n", error)) {
        return false;
    }
    kept_ = std::move(next);
    return true;
}

} // namespace opensu::profiles
