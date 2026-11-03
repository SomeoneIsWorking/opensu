// top_bar_chrome — the top bar as a chrome screen of the navigator: the HUD draws it, and its
// breadcrumb levels, launcher badges and touch buttons (Back, the menu) are parts the pointer
// lights and presses. Which of them take the pointer now is the app's to say each frame.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "gpu2d/ui/screen.hpp"

#include "hud.hpp"

namespace opensu::ui {

class TopBarChrome final : public gpu2d::ui::Screen {
  public:
    /// Which parts take the pointer this frame.
    struct Live {
        bool crumbs{false};
        bool launchers{false};
        /// The touch buttons the HUD shows (`Hud::setTouchButtons`).
        bool buttons{false};
    };
    struct Hooks {
        /// A press on level `index` of the trail.
        std::function<void(std::size_t)> crumb;
        /// A press on a launcher's badge.
        std::function<void(const LauncherBadge&)> launcher;
        /// A press on a touch button.
        std::function<void(TouchButton)> button;
    };

    explicit TopBarChrome(Hud& hud) : Screen{"topbar", "Top bar"}, hud_{hud} {
    }

    /// Set once by the app.
    void setHooks(Hooks hooks) {
        hooks_ = std::move(hooks);
    }
    void setLive(Live live) noexcept {
        live_ = live;
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;

  private:
    [[nodiscard]] static std::string crumbId(std::size_t index);
    [[nodiscard]] static std::string launcherId(std::size_t index);
    [[nodiscard]] static std::string buttonId(TouchButton button);
    void paint(gpu2d::ui::Painter& painter);

    Hud& hud_;
    Hooks hooks_;
    Live live_;
};

} // namespace opensu::ui
