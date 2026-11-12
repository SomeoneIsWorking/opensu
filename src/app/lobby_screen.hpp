// lobby_screen — Play online's public games on a card over the dimmed screen, reading
// `OnlinePlay::lobbies` each time it builds: where the search stands, a row per game with its
// seats, Refresh and Close. A picks a game; rows stand still while a host is being asked.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "online_play.hpp"

namespace opensu::app {

class LobbyScreen final : public gpu2d::ui::Screen {
  public:
    static constexpr const char* screenId = "lobbies";
    static constexpr const char* listId = "lobbies/list";
    static constexpr const char* refreshId = "lobbies/refresh";
    static constexpr const char* closeId = "lobbies/close";
    /// The id of game `index`'s row.
    [[nodiscard]] static std::string gameId(std::size_t index);

    explicit LobbyScreen(OnlinePlay& online) : Screen{screenId, "Public games"}, online_{online} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    OnlinePlay& online_;
    /// Games stood pickable at the last build.
    bool offered_{false};
};

} // namespace opensu::app
