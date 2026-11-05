// profile_book — the players of this openSU, `profiles.json` in the config dir: each profile's id,
// name, picture and the paired openSUs it is linked with (a copy of it there, whose saves sync with
// these), and which one is active. The first profile is made silently the first time, so there is
// always one. Names are kept as given; callers pass them through `lan::shownName`, the rule every
// name that crosses the LAN follows. Safe to use from any thread.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::profiles {

/// The file the profiles are kept in, under the config dir.
inline constexpr std::string_view profilesFile = "profiles.json";

/// A profile id is this many random bytes, written as lower-case hex.
inline constexpr std::size_t profileIdBytes = 16;

/// Whether `text` can be a profile id.
[[nodiscard]] bool isProfileId(std::string_view text) noexcept;

/// How many pictures a profile can have: colours (`pictureName`).
inline constexpr std::uint8_t pictureCount = 8;

/// The colour `picture` is, as Settings › Profiles names it; `picture` wraps at `pictureCount`.
[[nodiscard]] std::string_view pictureName(std::uint8_t picture) noexcept;

/// The first profile's name.
inline constexpr std::string_view firstProfileName = "Player 1";

struct Profile {
    std::string id;
    std::string name;
    /// Below `pictureCount`.
    std::uint8_t picture{0};
    /// The paired openSUs (peer ids) holding a linked copy of it.
    std::vector<std::string> links;

    [[nodiscard]] bool linkedWith(std::string_view peer) const;
    bool operator==(const Profile&) const = default;
};

class ProfileBook {
  public:
    /// Reads `file`, making and keeping the first profile when it holds none; throws
    /// `std::runtime_error` when a new first profile cannot be kept.
    explicit ProfileBook(std::filesystem::path file);

    /// In order: the first profile first.
    [[nodiscard]] std::vector<Profile> all() const;
    [[nodiscard]] std::optional<Profile> find(std::string_view id) const;
    [[nodiscard]] Profile first() const;
    [[nodiscard]] Profile active() const;
    /// `id`'s place in the order, which a Switch emulator's user follows; nothing when there is no
    /// such profile.
    [[nodiscard]] std::optional<std::size_t> seatOf(std::string_view id) const;
    /// The profile `id` linked with `peer`; nothing when it is not here or not linked with it.
    [[nodiscard]] std::optional<Profile> linked(std::string_view id, std::string_view peer) const;

    // Each change below is kept before it counts; false with `error` when it cannot be, the book
    // then as it was.
    [[nodiscard]] bool activate(std::string_view id, std::string& error);
    /// A new profile named `name`, with the next picture; nothing with `error`.
    [[nodiscard]] std::optional<Profile> add(std::string name, std::string& error);
    [[nodiscard]] bool rename(std::string_view id, std::string name, std::string& error);
    [[nodiscard]] bool setPicture(std::string_view id, std::uint8_t picture, std::string& error);
    /// `peer` holds a linked copy of `id` from now on, or no longer.
    [[nodiscard]] bool link(std::string_view id, std::string_view peer, bool on,
                            std::string& error);
    /// `copy`, the profile of another openSU `peer` allowed this one to copy, linked with it: added
    /// with that id, or linked again when a profile with the id is here already (its name and
    /// picture kept).
    [[nodiscard]] bool adopt(const Profile& copy, std::string_view peer, std::string& error);
    /// Drops every link with `peer`, which is no longer paired.
    [[nodiscard]] bool forgetPeer(std::string_view peer, std::string& error);

  private:
    struct Kept {
        std::vector<Profile> profiles;
        std::string active;
    };
    /// The profile `id`, or null; under `mutex_`, or before another thread has the book.
    [[nodiscard]] const Profile* at(std::string_view id) const;
    /// Writes `next` and takes it; under `mutex_`.
    [[nodiscard]] bool keep(Kept next, std::string& error);

    std::filesystem::path file_;
    mutable std::mutex mutex_;
    Kept kept_;
};

} // namespace opensu::profiles
