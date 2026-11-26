// quick_settings_panel — the Quick settings panel: a toolkit overlay down the right edge, over the
// home screen or a running game, holding the settings the player picked (`SystemControls::shown`)
// and the way to Settings › Quick settings to pick them. The Quick settings shortcut opens and
// closes it; Back, Start or a tap beside it closes it. The game is not held while it is up, so a
// change is heard and seen as it is made.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/screen.hpp"

#include "audio/sdl_sound_player.hpp"
#include "system_controls.hpp"
#include "ui/row_widgets.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class QuickSettingsPanel final : public ui::RowActions {
  public:
    struct Hooks {
        /// Opens Settings › Quick settings in the panel's place.
        std::function<void()> choose;
    };

    QuickSettingsPanel(gpu2d::ui::Navigator& navigator, SystemControls& settings,
                       audio::SdlSoundPlayer& sounds, Hooks hooks)
        : navigator_{navigator}, settings_{settings}, sounds_{sounds}, hooks_{std::move(hooks)} {
    }

    /// Opens the panel, or closes it when open.
    void toggle();
    /// Closes the panel and what is over it.
    void close();
    [[nodiscard]] bool isOpen() const;

    /// The picked settings' rows, then the way to pick them.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;
    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;

  private:
    [[nodiscard]] const gpu2d::ui::Screen* panel() const;

    gpu2d::ui::Navigator& navigator_;
    SystemControls& settings_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
};

class QuickSettingsScreen final : public gpu2d::ui::Screen {
  public:
    explicit QuickSettingsScreen(QuickSettingsPanel& panel)
        : Screen{"quickSettings", "Quick settings"}, panel_{panel} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    QuickSettingsPanel& panel_;
};

} // namespace opensu::app
