// setting_sheet — one layered setting over the dimmed screen: a row for each level of the editor's
// context that may keep it under "Override for", the most specific first (Alien vs Predator,
// FinalBurn Neo, Arcade, All games), each with the value it keeps or inherits, and a "Remove
// override" row for each level that keeps one.
#pragma once

#include <map>
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
    /// The row setting the value `level` keeps.
    void addLevel(gpu2d::ui::Column& rows, const settings::ScopedValue& value,
                  const settings::Layer& level);
    /// The value at `level`, or null when the setting is not kept there.
    [[nodiscard]] const settings::ScopedValue*
    find(const std::vector<settings::ScopedValue>& values) const;
    /// The context's level of `kind`.
    [[nodiscard]] const settings::Layer& layerOf(settings::Layer::Kind kind) const;
    /// "Alien vs Predator" for the game's level, else the level's name.
    [[nodiscard]] std::string levelName(const settings::Layer& level) const;
    /// The level's row under "Override for": "FinalBurn Neo", or "All games".
    [[nodiscard]] std::string levelTitle(const settings::Layer& level) const;
    /// "Use 4:3 from Arcade", or "Use the default: Off".
    [[nodiscard]] std::string useText(const settings::ScopedValue& value) const;

    LayeredSettings& settings_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    EditorContext context_;
    EditorLook look_;
    settings::Schema schema_;
    std::string key_;
    /// Each level's choice keys by place, place 0 inheriting; kept here so a row's closure holds
    /// only values that copy without throwing.
    std::map<settings::Layer::Kind, std::vector<std::string>> choiceKeys_;
};

} // namespace opensu::app
