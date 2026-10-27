// row_widgets — a `SettingsRow` as the interface toolkit's widget of its kind, the one mapping
// every screen of rows uses: a switch flips on A, a choice steps on Left and Right and lists its
// options on A, a slider steps on Left and Right, a folder or page row opens on A, and a row that
// throws something away asks first. What a row does goes to its owner through `RowActions`.
#pragma once

#include <string>

#include "gpu2d/ui/layout.hpp"

#include "setting_row.hpp"

namespace opensu::ui {

/// What a screen's rows ask of the owner of their model.
class RowActions {
  public:
    RowActions() = default;
    virtual ~RowActions() = default;
    RowActions(const RowActions&) = delete;
    RowActions& operator=(const RowActions&) = delete;
    RowActions(RowActions&&) = delete;
    RowActions& operator=(RowActions&&) = delete;

    /// Acts on row `id`: `step` is 0 for A, else how many places a choice moves.
    virtual void run(const std::string& id, int step) = 0;
    /// Sets the slider `id` to `level`.
    virtual void setLevel(const std::string& /*id*/, int /*level*/) {
    }
    /// Opens the page row `id` leads to.
    virtual void openPage(const std::string& /*id*/) {
    }
    /// X on row `id`, which names a second action.
    virtual void secondary(const std::string& /*id*/) {
    }
};

/// Adds the widget `row` stands for to `rows`.
void addRow(gpu2d::ui::Column& rows, const SettingsRow& row, RowActions& actions);

} // namespace opensu::ui
