// layered_editor — the one editor of layered settings, for a game (its own level over its core's,
// its console's and the one for all games), Global, a console or a core. It opens on its categories
// (`settings::sectionOrder`), each a row leading to that category's page; a core's option groups
// lead on again to a page each. A setting's row shows the value in effect and a chip naming the
// level it comes from; A opens the setting's sheet (`SettingSheet`), which sets it for all games
// and adds or removes an override at each level. Frame generation and resolution and upscaling
// open on a one-line summary. As a page it fills the screen (Settings, a game's details); as a
// panel it stands at the right over the running game. The shader preset opens `ShaderPresetScreen`,
// the pack's folders in the same presentation.
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

/// The rows of an editor that lead on to its pages: a row per category, a category's or a group's
/// page, the game's own rows and Clear. It holds what it is given by reference, so it lives as long
/// as the rows it adds: as a member of the editor, or of Quick settings' Game tab.
class EditorPages {
  public:
    EditorPages(LayeredSettings& settings, gpu2d::ui::Navigator& navigator,
                const ui::Chrome& chrome, const EditorContext& context, EditorLook look,
                const settings::Schema& schema, const EditorRows& more)
        : settings_{settings}, navigator_{navigator}, chrome_{chrome}, context_{context},
          look_{look}, schema_{schema}, more_{more} {
    }

    /// The values `addCategories` lists: every setting the schema offers at the context's level.
    [[nodiscard]] std::vector<settings::ScopedValue> values() const;
    /// A row per category with a setting in `values`, each opening its page; the game's own rows,
    /// as a page of their own when there are settings too; then Clear. The first row's id.
    std::string addCategories(gpu2d::ui::Column& rows,
                              const std::vector<settings::ScopedValue>& values) const;
    /// The row opening the page of `section` (and `group` in it), with how many of its settings
    /// are changed.
    void addPage(gpu2d::ui::Column& rows, const std::vector<settings::ScopedValue>& values,
                 const std::string& section, const std::string& group) const;

  private:
    void addClear(gpu2d::ui::Column& rows) const;

    LayeredSettings& settings_;
    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    const EditorContext& context_;
    EditorLook look_;
    const settings::Schema& schema_;
    const EditorRows& more_;
};

class LayeredEditor final : public gpu2d::ui::Screen {
  public:
    /// `title` names what it edits. Without `section` it lists the categories; with it, that
    /// category's settings; with `group` too, only that group's (`Setting::group`).
    LayeredEditor(LayeredSettings& settings, gpu2d::ui::Navigator& navigator,
                  const ui::Chrome& chrome, EditorContext context, EditorLook look,
                  std::string title, EditorRows more = {}, std::string section = {},
                  std::string group = {});

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
    /// The page's rows into `rows`: the categories, a category's settings or a group's.
    void addRows(gpu2d::ui::Column& rows, const std::vector<settings::ScopedValue>& values);
    void addRow(gpu2d::ui::Column& rows, const settings::ScopedValue& value);
    /// Whether `value` stands on this page, or on one it leads to.
    [[nodiscard]] bool below(const settings::ScopedValue& value) const;
    /// The summary over `section`'s rows, for a section that has one.
    void addSummary(gpu2d::ui::Column& rows, const std::string& section);
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
    EditorPages pages_;
    /// The category this page sets; empty for the list of categories.
    std::string section_;
    /// The group of `section_` this page sets; empty for the whole category.
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
