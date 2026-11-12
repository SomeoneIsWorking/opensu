// online_screen — Play online's card over the dimmed screen, reading `OnlinePlay` each time it
// builds: where things stand, the code with Copy, List publicly or how the listing stands, the
// host's field for a reply, Cancel or Close.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "online_play.hpp"

namespace opensu::app {

class OnlineScreen final : public gpu2d::ui::Screen {
  public:
    static constexpr const char* screenId = "online";
    static constexpr const char* fieldId = "online/reply";
    static constexpr const char* copyId = "online/copy";
    static constexpr const char* listId = "online/list";
    static constexpr const char* closeId = "online/close";

    explicit OnlineScreen(OnlinePlay& online) : Screen{screenId, "Play online"}, online_{online} {
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
    std::string codeSeen_;
};

} // namespace opensu::app
