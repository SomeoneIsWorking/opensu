// layered_editor — the one editor of layered settings, for a game (its own level over its core's,
// its console's and the one for all games), Global, a console or a core. Each row shows the value
// in effect and a chip naming the level it comes from; A opens the setting's sheet
// (`SettingSheet`), which overrides or removes it at each level. A core's option categories each
// open the editor on their own options. Frame generation and resolution and upscaling open on a
// one-line summary. As a page it fills the screen (Settings, a game's details); as a panel it
// stands at the right over the running game. The shader preset opens `ShaderPresetScreen`, the
// pack's folders in the same presentation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/screen.hpp"

#include "layered_settings.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

enum class EditorLook : std::uint8_t {
    /// Fills the screen under the top bar.
    Page,
    /// A panel at the right, the running game showing beside it.
    Panel,
};

/// Rows of a game beyond its settings, which the game's own tab shows after them.
struct EditorRows {
    std::function<std::vector<ui::SettingsRow>()> rows;
    /// What they do; outlives the editor.
    ui::RowActions* actions{nullptr};
};

class LayeredEditor final : public gpu2d::ui::Screen {
  public:
    /// `title` names what it edits; with `group`, only that group's settings (`Setting::group`).
    LayeredEditor(LayeredSettings& settings, gpu2d::ui::Navigator& navigator,
                  const ui::Chrome& chrome, EditorContext context, EditorLook look,
                  std::string title, EditorRows more = {}, std::string group = {});

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return look_ == EditorLook::Panel;
    }
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    /// The level the editor was opened for: the game, else the console or core.
    [[nodiscard]] const settings::Layer& scope() const {
        return context_.scopes.back();
    }
    /// The rows, under their headings, into `rows`.
    void addRows(gpu2d::ui::Column& rows, const gpu2d::ui::Theme& theme);
    void addRow(gpu2d::ui::Column& rows, const settings::ScopedValue& value);
    /// The row opening `group`'s page, with how many of its settings are changed.
    void addGroup(gpu2d::ui::Column& rows, const std::vector<settings::ScopedValue>& values,
                  const std::string& group);
    /// The summary over `section`'s rows, for a section that has one.
    void addSummary(gpu2d::ui::Column& rows, const std::string& section);
    void addClear(gpu2d::ui::Column& rows);
    LayeredSettings& settings_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    EditorContext context_;
    EditorLook look_;
    /// What may be set here, read once as the editor opens.
    settings::Schema schema_;
    /// The first row as last built.
    std::string firstRow_;
    EditorRows more_;
    /// The group this page sets; empty for the editor itself.
    std::string group_;
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
