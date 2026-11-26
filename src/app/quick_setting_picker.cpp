#include "quick_setting_picker.hpp"

#include <algorithm>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "quick_settings_panel.hpp"
#include "ui/icon_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The screens' width, in dp: the panel's.
constexpr float pickerDp = 360.0f;

gpu2d::Rect rightEdge(const gpu2d::Rect& viewport, const tk::Theme& theme) {
    const float width = std::min(viewport.width, theme.px(pickerDp + theme.spacing.pad * 3.0f));
    return gpu2d::Rect{viewport.x + viewport.width - width, viewport.y, width, viewport.height};
}

} // namespace

std::string QuickSettingPicker::title() const {
    return pages_.empty() ? Screen::title() : panel_.settings().pageTitle(pages_.back());
}

std::unique_ptr<tk::Widget> QuickSettingPicker::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Backdrop>();
    root->pad(tk::Insets::all(spacing.pad * 1.5f)).space(spacing.pad);
    root->add<tk::Label>(title(), tk::TextRole::Title);
    tk::List& list = root->add<tk::List>("quickSettings/add/rows", spacing.gap);
    list.grow();
    tk::Column& rows = list.rows();
    if (pages_.empty()) {
        for (const ui::SettingsCategory& category : panel_.settings().categories()) {
            rows.add<tk::Item>("quickSettings/add/page/" + category.id,
                               tk::Item::Text{category.label, {}})
                .onPress(
                    [this, id = category.id] {
                        pages_.push_back(id);
                    },
                    "Open", true);
        }
    } else {
        const std::string page = pages_.back();
        const settings::QuickLayout layout = panel_.layout().layout();
        const std::vector<settings::QuickPin>* held = nullptr;
        if (tab_ < layout.tabs.size() && category_ < layout.tabs[tab_].categories.size()) {
            held = &layout.tabs[tab_].categories[category_].items;
        }
        for (const ui::SettingsRow& row : panel_.settings().page(page).rows) {
            if (row.kind == ui::RowKind::Heading) {
                rows.add<tk::SectionHeading>(row.label);
                continue;
            }
            if (row.kind == ui::RowKind::Page) {
                rows.add<tk::Item>("quickSettings/add/page/" + row.id,
                                   tk::Item::Text{row.label, row.note})
                    .value(row.value)
                    .onPress(
                        [this, id = row.id] {
                            pages_.push_back(id);
                        },
                        "Open", true);
                continue;
            }
            if (!QuickLayoutEditor::pinnable(row)) {
                continue;
            }
            const settings::QuickPin pin{page, row.id, row.label};
            const bool added =
                held != nullptr && (std::ranges::find(*held, pin) != std::ranges::end(*held));
            tk::Item& item = rows.add<tk::Item>("quickSettings/add/row/" + row.id,
                                                tk::Item::Text{row.label, row.note});
            if (added) {
                item.value("Added");
                continue;
            }
            item.onPress(
                [this, pin, label = row.label] {
                    panel_.layout().addItem(tab_, category_, pin);
                    panel_.say("Added " + label + " to Quick settings");
                    close();
                },
                "Add");
        }
    }
    root->add<tk::HintBar>();
    return root;
}

gpu2d::Rect QuickSettingPicker::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    return rightEdge(viewport, theme);
}

tk::Screen::BackReply QuickSettingPicker::back(tk::ActContext& /*context*/) {
    if (pages_.empty()) {
        return BackReply::Pop;
    }
    pages_.pop_back();
    return BackReply::Stay;
}

std::vector<tk::Hint> QuickSettingPicker::hints() const {
    return {tk::Hint{{tk::Action::Back}, pages_.empty() ? "Cancel" : "Back"}};
}

std::unique_ptr<tk::Widget> QuickIconScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Backdrop>();
    root->pad(tk::Insets::all(spacing.pad * 1.5f)).space(spacing.pad);
    root->add<tk::Label>(title(), tk::TextRole::Title);
    tk::List& list = root->add<tk::List>("quickSettings/icon/rows", spacing.gap);
    list.grow();
    for (const QuickIcon icon : allQuickIcons) {
        std::string name{key(icon)};
        name.front() = static_cast<char>(name.front() - 'a' + 'A');
        list.rows()
            .add<ui::IconItem>("quickSettings/icon/" + std::string{key(icon)},
                               tk::Item::Text{name, {}}, panel_.images(), iconOf(icon))
            .onPress([this, icon] {
                panel_.layout().setIcon(tab_, icon);
                close();
            });
    }
    root->add<tk::HintBar>();
    return root;
}

gpu2d::Rect QuickIconScreen::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    return rightEdge(viewport, theme);
}

} // namespace opensu::app
