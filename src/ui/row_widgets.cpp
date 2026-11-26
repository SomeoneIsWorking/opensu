#include "row_widgets.hpp"

#include <cstddef>
#include <utility>

#include "gpu2d/ui/widgets.hpp"

#include "icon_widgets.hpp"

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

/// The row's item: with the stream icon for a Play that streams, when the screen has icons.
tk::Item& itemOf(tk::Column& rows, const SettingsRow& row, ScreenImages* images) {
    tk::Item::Text text{row.label, row.note};
    if (row.streams && images != nullptr) {
        return rows.add<IconItem>(row.id, std::move(text), *images, Icon::Stream);
    }
    return rows.add<tk::Item>(row.id, std::move(text));
}

void add(tk::Column& rows, const SettingsRow& row, RowActions& actions, ScreenImages* images);

} // namespace

void addRow(tk::Column& rows, const SettingsRow& row, RowActions& actions) {
    add(rows, row, actions, nullptr);
}

void addRow(tk::Column& rows, const SettingsRow& row, RowActions& actions, ScreenImages& images) {
    add(rows, row, actions, &images);
}

namespace {

void add(tk::Column& rows, const SettingsRow& row, RowActions& actions, ScreenImages* images) {
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
                             row.level,
                             [&actions, id = row.id](int level) {
                                 actions.setLevel(id, level);
                             })
            .unit(row.unit);
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
            itemOf(rows, row, images).value(row.value).asButton().onPress([&actions, id = row.id] {
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

} // namespace

} // namespace opensu::ui
