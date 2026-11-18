// setting_sheet — one layered setting over the dimmed screen, least specific first: its value for
// all games, then each level of the editor's context that overrides it (Libretro, Arcade,
// FinalBurn Neo, Alien vs Predator), each with an × (or X) that removes the override, then "Add
// override" with a button for each level that has none yet.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/screen.hpp"

#include "layered_editor.hpp"
#include "layered_settings.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

/// `value` of `setting` as a row reads it.
[[nodiscard]] std::string valueLabel(const settings::Setting& setting, const std::string& value);

class SettingSheet final : public gpu2d::ui::Screen {
  public:
    /// `key` is a setting of `context`'s schema, `label` what it is called.
    SettingSheet(LayeredSettings& settings, gpu2d::ui::Navigator& navigator,
                 const ui::Chrome& chrome, EditorContext context, EditorLook look, std::string key,
                 std::string label);

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    /// The row setting the value `level` keeps, with its × `gap` dp beside it when it keeps one.
    void addLevel(gpu2d::ui::Column& rows, const settings::ScopedValue& value,
                  const settings::Layer& level, float gap);
    /// The button adding an override at `level`, or putting back the one removed this run.
    void addOverride(gpu2d::ui::Column& rows, const settings::ScopedValue& value,
                     const settings::Layer& level);
    /// The value at `level`, or null when the setting is not kept there.
    [[nodiscard]] const settings::ScopedValue*
    find(const std::vector<settings::ScopedValue>& values) const;
    /// The context's level of `kind`.
    [[nodiscard]] const settings::Layer& layerOf(settings::Layer::Kind kind) const;
    /// "Alien vs Predator" for the game's level, else the level's name.
    [[nodiscard]] std::string levelName(const settings::Layer& level) const;
    /// The level's row: "FinalBurn Neo", or "All games".
    [[nodiscard]] std::string levelTitle(const settings::Layer& level) const;

    LayeredSettings& settings_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    EditorContext context_;
    EditorLook look_;
    settings::Schema schema_;
    std::string key_;
    /// The setting's choice keys by place; kept here so a row's closure holds only values that
    /// copy without throwing.
    std::vector<std::string> choiceKeys_;
    /// The level focused when it opens: the one whose value is in effect.
    std::string focus_;
};

} // namespace opensu::app
