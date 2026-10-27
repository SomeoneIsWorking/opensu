#include "row_widgets.hpp"

#include <cstddef>

#include "gpu2d/ui/widgets.hpp"

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

/// The first word of `label`: what a question's button does ("Remove", "Close").
std::string verbOf(const std::string& label) {
    return label.substr(0, label.find(' '));
}

/// Gives `item` the row's second action, when it has one.
void addSecondary(tk::Item& item, const SettingsRow& row, RowActions& actions) {
    if (!row.secondary.empty()) {
        item.onAction(
            tk::Action::Secondary,
            [&actions, id = row.id] {
                actions.secondary(id);
            },
            row.secondary);
    }
}

} // namespace

void addRow(tk::Column& rows, const SettingsRow& row, RowActions& actions) {
    const tk::Item::Text text{row.label, row.note};
    switch (row.kind) {
    case RowKind::Heading:
        rows.add<tk::SectionHeading>(row.label);
        return;
    case RowKind::Toggle:
        rows.add<tk::Toggle>(row.id, text, row.on, [&actions, id = row.id](bool) {
            actions.run(id, 0);
        });
        return;
    case RowKind::Choice:
        if (row.choices.size() < 2) {
            rows.add<tk::Item>(row.id, text).value(row.value);
            return;
        }
        rows.add<tk::Choice>(row.id, text, row.choices, row.chosen,
                             [&actions, id = row.id, chosen = row.chosen](std::size_t picked) {
                                 if (picked != chosen) {
                                     actions.run(id, static_cast<int>(picked) -
                                                         static_cast<int>(chosen));
                                 }
                             });
        return;
    case RowKind::Slider:
        rows.add<tk::Slider>(row.id, text, tk::Slider::Range{row.low, row.high, row.step},
                             row.level, [&actions, id = row.id](int level) {
                                 actions.setLevel(id, level);
                             });
        return;
    case RowKind::Folder:
        rows.add<tk::Item>(row.id, text)
            .value(row.value)
            .onPress(
                [&actions, id = row.id] {
                    actions.run(id, 0);
                },
                "Choose", true);
        return;
    case RowKind::Page:
        rows.add<tk::Item>(row.id, text)
            .value(row.value)
            .onPress(
                [&actions, id = row.id] {
                    actions.openPage(id);
                },
                "Open", true);
        return;
    case RowKind::Action: {
        tk::Item& item =
            rows.add<tk::Item>(row.id, text).value(row.value).onPress([&actions, id = row.id] {
                actions.run(id, 0);
            });
        if (!row.confirm.empty()) {
            item.confirmFirst(row.label + "?", row.confirm,
                              row.confirmVerb.empty() ? verbOf(row.label) : row.confirmVerb);
        }
        addSecondary(item, row, actions);
        return;
    }
    case RowKind::Info:
        rows.add<tk::Item>(row.id, text).value(row.value);
        return;
    }
}

} // namespace opensu::ui
