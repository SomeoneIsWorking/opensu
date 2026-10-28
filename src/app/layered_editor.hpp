// layered_editor — the one editor of layered settings, for any scope: a game's (tabs for Global,
// its console, its core and the game), Global, a console or a core. Each row shows the value in
// effect at the scope and a chip naming where it comes from; choosing a value keeps it at the
// scope, and every list starts with "Use <value> from <layer>", which removes the scope's own. X
// goes to the scope the value comes from, Y removes the scope's value or restores one removed this
// run. Frame generation and resolution and upscaling open on a one-line summary. As a page it fills
// the screen (Settings, a game's details); as a panel it stands at the right over the running game.
// The shader preset's row opens `ShaderPresetScreen`, the pack's folders in the same presentation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/screen.hpp"

#include "layered_settings.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

enum class EditorLook : std::uint8_t {
    /// Fills the screen under the top bar.
    Page,
    /// A panel at the right, the running game showing beside it.
    Panel,
};

class LayeredEditor final : public gpu2d::ui::Screen {
  public:
    /// `title` names what it edits.
    LayeredEditor(LayeredSettings& settings, gpu2d::ui::Navigator& navigator,
                  const ui::Chrome& chrome, EditorContext context, EditorLook look,
                  std::string title);

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return look_ == EditorLook::Panel;
    }
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override;

    /// The scope on show, as its tab names it.
    [[nodiscard]] std::string scopeName() const;
    /// Shows the scope `index` of the tabs.
    void showScope(std::size_t index);

  private:
    [[nodiscard]] const settings::Layer& scope() const {
        return context_.scopes[scope_];
    }
    /// The rows of the scope on show, under their headings, into `rows`.
    void addRows(gpu2d::ui::Column& rows, const gpu2d::ui::Theme& theme);
    void addRow(gpu2d::ui::Column& rows, const settings::ScopedValue& value);
    /// The frame generation summary over its rows.
    void addFrameGenSummary(gpu2d::ui::Column& rows);
    /// The resolution and upscaling summary over its rows.
    void addScalingSummary(gpu2d::ui::Column& rows);
    void addClear(gpu2d::ui::Column& rows);
    /// `value` of `setting` as the row reads it.
    [[nodiscard]] std::string labelOf(const settings::Setting& setting,
                                      const std::string& value) const;
    /// "Use 4:3 from Global", or "Use the default: Off".
    [[nodiscard]] std::string useText(const settings::ScopedValue& value) const;
    /// Moves to the tab of `layer`, keeping the row focused.
    void goTo(const settings::Layer& layer);

    LayeredSettings& settings_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    EditorContext context_;
    EditorLook look_;
    /// What may be set here, read once as the editor opens.
    settings::Schema schema_;
    std::size_t scope_;
    /// The first row as last built.
    std::string firstRow_;
};

/// The shader pack's folders for one scope's Shader preset: "Use … from …" and None at its top,
/// then each folder's subfolders and presets. A pick keeps it and closes the picker.
class ShaderPresetScreen final : public gpu2d::ui::Screen {
  public:
    ShaderPresetScreen(LayeredSettings& settings, gpu2d::ui::Navigator& navigator,
                       const ui::Chrome& chrome, EditorContext context, settings::Layer scope,
                       EditorLook look);

    [[nodiscard]] std::string title() const override;
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return look_ == EditorLook::Panel;
    }
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    void pick(const std::string* value);
    /// Shows `folder`, its first row focused.
    void open(const std::string& folder);

    LayeredSettings& settings_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    EditorContext context_;
    settings::Layer scope_;
    EditorLook look_;
    std::vector<library::roms::ShaderPreset> presets_;
    /// The open folder's path in the pack, "" for its top.
    std::string folder_;
};

} // namespace opensu::app
