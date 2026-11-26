// quick_settings_panel — the Quick settings panel: a toolkit overlay down the right edge, over the
// home screen or a running game. Its tabs show as icons; the chosen tab's categories each head the
// items under them, and an item is a live Settings row (`QuickLayoutEditor`'s pins, looked up
// through `SettingsController`, which also does what the row does). Customize arranges tabs,
// categories and items by grabbing and moving them, removes them, renames a category, changes a
// tab's icon and adds any Settings row through Add a setting. The Quick settings shortcut opens and
// closes it; Back, Start or a tap beside it closes it. The game is not held while it is up, so a
// change is heard and seen as it is made.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/screen.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "quick_layout.hpp"
#include "settings_controller.hpp"
#include "system_controls.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_images.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class QuickSettingsPanel final : public ui::RowActions {
  public:
    struct Services {
        QuickLayoutEditor& layout;
        SettingsController& settings;
        /// Read as the panel opens: the brightness and power profile its rows show.
        SystemControls& system;
        ui::ScreenImages& images;
        KeyboardPad& keyboard;
    };

    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    QuickSettingsPanel(gpu2d::ui::Navigator& navigator, Services services,
                       audio::SdlSoundPlayer& sounds, Hooks hooks)
        : navigator_{navigator}, layout_{services.layout}, settings_{services.settings},
          system_{services.system}, images_{services.images}, keyboard_{services.keyboard},
          sounds_{sounds}, hooks_{std::move(hooks)} {
    }

    /// Opens the panel, or closes it when open.
    void toggle();
    /// Opens the panel on tab `tab`, in Customize when `customize`; open already, it shows that.
    void open(std::size_t tab, bool customize);
    /// Closes the panel and what is over it.
    void close();
    [[nodiscard]] bool isOpen() const;

    // What the screens ask.
    [[nodiscard]] QuickLayoutEditor& layout() noexcept {
        return layout_;
    }
    [[nodiscard]] SettingsController& settings() noexcept {
        return settings_;
    }
    [[nodiscard]] ui::ScreenImages& images() noexcept {
        return images_;
    }
    /// The Settings row `pin` names as it stands now; nothing when this machine does not show it
    /// or the panel cannot hold it.
    [[nodiscard]] std::optional<ui::SettingsRow> rowOf(const settings::QuickPin& pin);
    /// The entry card renaming the category.
    void rename(std::size_t tab, std::size_t category);
    /// The list of icons for the tab.
    void chooseIcon(std::size_t tab);
    /// Browses Settings for a row to add to the category.
    void addSetting(std::size_t tab, std::size_t category);
    /// Tells the player something.
    void say(const std::string& text) const;

    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;

  private:
    [[nodiscard]] const gpu2d::ui::Screen* panel() const;
    void closeEntry();

    gpu2d::ui::Navigator& navigator_;
    QuickLayoutEditor& layout_;
    SettingsController& settings_;
    SystemControls& system_;
    ui::ScreenImages& images_;
    KeyboardPad& keyboard_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    /// The category the entry card renames, and its name as the card edits it.
    std::size_t namingTab_{};
    std::size_t namingCategory_{};
    std::string draft_;
};

class QuickSettingsScreen final : public gpu2d::ui::Screen {
  public:
    QuickSettingsScreen(QuickSettingsPanel& panel, std::size_t tab, bool customize)
        : Screen{"quickSettings", "Quick settings"}, panel_{panel}, tab_{tab}, editing_{customize} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    /// The first setting of the first tab; its tab when it has none.
    [[nodiscard]] std::string initialFocus() const override;

  private:
    /// What is held while Customize moves it.
    struct Grab {
        enum class Kind : std::uint8_t { Tab, Category, Item };
        Kind kind{Kind::Item};
        ItemPlace place;
        /// The layout before the grab, put back by B.
        settings::QuickLayout before;
    };

    void buildTabs(gpu2d::ui::Column& root, const gpu2d::ui::Theme& theme,
                   const settings::QuickLayout& layout);
    void buildUse(gpu2d::ui::Column& rows, const gpu2d::ui::Theme& theme,
                  const settings::QuickTab& tab);
    void buildEdit(gpu2d::ui::Column& rows, const gpu2d::ui::Theme& theme,
                   const settings::QuickTab& tab);
    /// A press on a row or tab in Customize: grabs it, drops what is held, or drops it there.
    void press(Grab::Kind kind, ItemPlace place);
    /// Moves what is held `step` places.
    void move(int step, gpu2d::ui::ActContext& context);
    /// Removes the focused tab, category or item.
    void removeFocused();
    /// The focused row's place in Customize; nothing for another widget.
    [[nodiscard]] std::optional<std::pair<Grab::Kind, ItemPlace>> focusedPlace() const;
    [[nodiscard]] std::size_t tabCount() const;

    QuickSettingsPanel& panel_;
    std::size_t tab_{};
    bool editing_{false};
    std::optional<Grab> grab_;
};

} // namespace opensu::app
