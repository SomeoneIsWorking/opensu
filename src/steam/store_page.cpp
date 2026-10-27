#include "store_page.hpp"

#include <algorithm>
#include <optional>
#include <vector>

#include "lucent/log.h"

namespace opensu::steam {
namespace {

using Clock = std::chrono::steady_clock;

bool onStore(const DevToolsTarget& target) {
    return target.type == "page" && target.url.starts_with(storeOrigin);
}

} // namespace

StorePage::StorePage(std::uint16_t port, StorePageTiming timing)
    : port_{port}, timing_{timing}, shared_{port} {
}

bool StorePage::show(const std::string& url, const std::stop_token& stop, std::string& error) {
    const Clock::time_point opening = Clock::now() + timing_.open;
    BigPicture::Leave leave{bigPicture_};
    if (!bigPicture_.show(stop, opening, timing_.poll, leave.opened, error)) {
        return false;
    }
    const std::optional<std::vector<DevToolsTarget>> before = devToolsTargets(port_, error);
    if (!before) {
        return false;
    }
    if (!openInSteamBrowser(shared_, url, error)) {
        return false;
    }
    lucent::info("steam", "store page asked for in Big Picture: {}", url);
    // The view: a store page that was not there before.
    std::optional<std::string> view;
    const Clock::time_point until = Clock::now() + timing_.limit;
    while (rest(stop, timing_.poll)) {
        const std::optional<std::vector<DevToolsTarget>> now = devToolsTargets(port_, error);
        if (!now) {
            return false;
        }
        if (!view) {
            const auto opened = std::ranges::find_if(*now, [&before](const DevToolsTarget& target) {
                return onStore(target) &&
                       std::ranges::find(*before, target.id, &DevToolsTarget::id) == before->end();
            });
            if (opened != now->end()) {
                view = opened->id;
            } else if (Clock::now() > opening) {
                error = "Steam did not open the store page";
                return false;
            }
            continue;
        }
        const auto still = std::ranges::find(*now, *view, &DevToolsTarget::id);
        if (still == now->end() || !onStore(*still)) {
            lucent::info("steam", "store page left");
            return true;
        }
        if (Clock::now() > until) {
            return true;
        }
    }
    error = "cancelled";
    return false;
}

} // namespace opensu::steam
