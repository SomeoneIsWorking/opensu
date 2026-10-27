// folder_chooser_screen — the folder chooser on the interface toolkit: a tall card over the dimmed
// screen with the request's title, the path field, then Use this folder (when a folder is asked
// for), the entry that drops the setting, Up one level, the folders under the one shown and the
// files to pick (when a file is). Left goes up a level; B cancels.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "path_editor.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class FolderChooserScreen final : public gpu2d::ui::Screen {
  public:
    static constexpr const char* fieldId = "folders/path";
    static constexpr const char* useId = "folders/use";

    FolderChooserScreen(PathEditor& editor, const ui::Chrome& chrome)
        : Screen{"folders", editor.request().title}, editor_{editor}, chrome_{chrome} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override {
        return landing(editor_);
    }

    /// Where focus lands on a folder shown: Use this folder, else the first file, else Up one
    /// level, else the field.
    [[nodiscard]] static std::string landing(const PathEditor& editor);

  private:
    PathEditor& editor_;
    const ui::Chrome& chrome_;
};

} // namespace opensu::app
