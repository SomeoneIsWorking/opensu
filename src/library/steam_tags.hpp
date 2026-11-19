// steam_tags — the store tags of the Steam games openSU shows, most voted first, as Steam's public
// `GetItems` gives them, asked in the background and kept in a file between runs. Auto's touch
// layout reads a game's genre from them.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "library/game.hpp"
#include "net/web_client.hpp"

namespace opensu::library {

class SteamTags {
  public:
    /// Kept in `file`, asked of the `GetItems` at `api`.
    SteamTags(std::filesystem::path file, std::string api);
    ~SteamTags();
    SteamTags(const SteamTags&) = delete;
    SteamTags& operator=(const SteamTags&) = delete;
    SteamTags(SteamTags&&) = delete;
    SteamTags& operator=(SteamTags&&) = delete;

    /// Asks for the tags of every app of `games` not known yet.
    void want(const std::vector<Game>& games);
    /// `app`'s tags, most voted first; nothing until they arrived.
    [[nodiscard]] std::optional<std::vector<std::uint32_t>> of(std::uint32_t app) const;

  private:
    void run(const std::stop_token& stop);
    void save() const;

    std::filesystem::path file_;
    std::string api_;
    net::WebClient web_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::map<std::uint32_t, std::vector<std::uint32_t>> known_;
    std::set<std::uint32_t> wanted_;
    /// Last, so it is stopped and joined before the rest goes.
    std::jthread worker_;
};

} // namespace opensu::library
