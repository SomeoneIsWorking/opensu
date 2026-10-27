// store_page — a Steam store page in Big Picture's browser on openSU's own Steam, for the player
// to claim a free game in: Big Picture shown first (opened and waited for when it is not up), the
// page opened with `steam://openurl`, then watched over DevTools until the player leaves it, after
// which Big Picture is closed again when this opened it. Nothing on the page is read or pressed.
#pragma once

#include <chrono>
#include <cstdint>
#include <stop_token>
#include <string>

#include "big_picture.hpp"
#include "devtools.hpp"

namespace opensu::steam {

/// Steam's store; the page counts as open while a view shows a page of it.
inline constexpr std::string_view storeOrigin = "https://store.steampowered.com/";

struct StorePageTiming {
    std::chrono::milliseconds poll{std::chrono::milliseconds{500}};
    /// How long Big Picture and the page have to come up.
    std::chrono::seconds open{std::chrono::seconds{60}};
    /// The longest the page is watched.
    std::chrono::minutes limit{std::chrono::minutes{30}};
};

class StorePage {
  public:
    explicit StorePage(std::uint16_t port = devToolsPort, StorePageTiming timing = {});

    /// Shows `url` (a page of `storeOrigin`) and returns once the player has left it. False with
    /// `error` when Steam cannot be asked, Big Picture or the page does not come up, or on `stop`.
    /// Blocks; run it off the frame loop.
    [[nodiscard]] bool show(const std::string& url, const std::stop_token& stop,
                            std::string& error);

  private:
    std::uint16_t port_;
    StorePageTiming timing_;
    DevTools shared_;
    BigPicture bigPicture_{shared_};
};

} // namespace opensu::steam
