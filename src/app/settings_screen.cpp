#include "settings_screen.hpp"

#include <algorithm>
#include <cstddef>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr const char* tabsId = "categories";

/// Adds the widget `row` stands for to `rows`.
void addRow(tk::Column& rows, const ui::SettingsRow& row, SettingsController& controller) {
    const tk::Item::Text text{row.label, row.note};
    switch (row.kind) {
    case ui::RowKind::Heading:
        rows.add<tk::SectionHeading>(row.label);
        return;
    case ui::RowKind::Toggle:
        rows.add<tk::Toggle>(row.id, text, row.on, [&controller, id = row.id](bool) {
            controller.run(id, 0);
        });
        return;
    case ui::RowKind::Choice:
        if (row.choices.size() < 2) {
            rows.add<tk::Item>(row.id, text).value(row.value);
            return;
        }
        rows.add<tk::Choice>(row.id, text, row.choices, row.chosen,
                             [&controller, id = row.id, chosen = row.chosen](std::size_t picked) {
                                 if (picked != chosen) {
                                     controller.run(id, static_cast<int>(picked) -
                                                            static_cast<int>(chosen));
                                 }
                             });
        return;
    case ui::RowKind::Slider:
        rows.add<tk::Slider>(row.id, text, tk::Slider::Range{row.low, row.high, row.step},
                             row.level, [&controller, id = row.id](int level) {
                                 controller.setLevel(id, level);
                             });
        return;
    case ui::RowKind::Folder:
        rows.add<tk::Item>(row.id, text)
            .value(row.value)
            .onPress(
                [&controller, id = row.id] {
                    controller.run(id, 0);
                },
                "Choose", true);
        return;
    case ui::RowKind::Page:
        rows.add<tk::Item>(row.id, text)
            .value(row.value)
            .onPress(
                [&controller, id = row.id] {
                    controller.openPage(id);
                },
                "Open", true);
        return;
    case ui::RowKind::Action:
        if (!row.confirm.empty()) {
            rows.add<tk::Item>(row.id, text).value(row.value).onPress([&controller, row] {
                controller.confirm(row);
            });
            return;
        }
        rows.add<tk::Item>(row.id, text).value(row.value).onPress([&controller, id = row.id] {
            controller.run(id, 0);
        });
        return;
    case ui::RowKind::Info:
        rows.add<tk::Item>(row.id, text).value(row.value);
        return;
    }
}

/// The screen's frame: the top bar's room, `body`, then the hint bar.
struct Frame {
    std::unique_ptr<tk::Backdrop> root;
    tk::Row* body{};
};

Frame frameFor(const tk::Theme& theme, const ui::Chrome& chrome) {
    Frame frame{std::make_unique<tk::Backdrop>(), nullptr};
    frame.root->space(theme.spacing.gap);
    frame.root->add<tk::Spacer>(0.0f).fixHeight(chrome.top / theme.scale);
    tk::Row& body = frame.root->add<tk::Row>();
    body.align(tk::Align::Stretch);
    body.pad(tk::Insets{theme.spacing.pad * 2.0f, 0.0f, theme.spacing.pad * 2.0f, 0.0f})
        .space(theme.spacing.pad);
    body.grow();
    frame.body = &body;
    frame.root->add<tk::HintBar>().fixHeight(theme.spacing.hintBarHeight);
    return frame;
}

/// The pane `category`'s rows are listed on, beside the side column.
void addRows(tk::Row& body, const tk::Theme& theme, const ui::SettingsCategory& category,
             SettingsController& controller) {
    tk::Card& pane = body.add<tk::Card>();
    pane.grow();
    pane.pad(tk::Insets::all(theme.spacing.gap * 2.0f));
    tk::List& list = pane.add<tk::List>("rows/" + category.id, theme.spacing.gap);
    list.grow();
    for (const ui::SettingsRow& row : category.rows) {
        addRow(list.rows(), row, controller);
    }
}

} // namespace

std::string SettingsScreen::tabId() const {
    return std::string{tabsId} + "/" + category_;
}

std::string SettingsScreen::categoryLabel() const {
    return label_;
}

std::unique_ptr<tk::Widget> SettingsScreen::build(const tk::Theme& theme) {
    const std::vector<ui::SettingsCategory>& categories = controller_.categories();
    std::vector<tk::Tabs::Tab> tabs;
    std::vector<std::string> keys;
    tabs.reserve(categories.size());
    keys.reserve(categories.size());
    std::size_t chosen = 0;
    for (const ui::SettingsCategory& category : categories) {
        if (category.id == category_) {
            chosen = tabs.size();
        }
        tabs.push_back(tk::Tabs::Tab{category.id, category.label});
        keys.push_back(category.id);
    }
    category_ = categories[chosen].id;
    label_ = categories[chosen].label;

    Frame frame = frameFor(theme, chrome_);
    tk::ScrollView& side = frame.body->add<tk::ScrollView>("categories/scroll");
    side.fixWidth(theme.spacing.sidebarWidth);
    side.content<tk::Tabs>(
        tabsId, std::move(tabs), chosen,
        [this, keys = std::move(keys)](std::size_t index) {
            category_ = keys[index];
        },
        tk::Orientation::Vertical);
    addRows(*frame.body, theme, categories[chosen], controller_);
    return std::move(frame.root);
}

tk::Screen::BackReply SettingsScreen::back(tk::ActContext& context) {
    // From the rows, Back goes to the categories first.
    if (!view().focused.starts_with(std::string{tabsId} + "/")) {
        context.focus(tabId());
        context.sound(tk::Sound::Move);
        return BackReply::Stay;
    }
    return controller_.leave();
}

bool SettingsScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    static_cast<void>(controller_.leave());
    return true;
}

std::vector<tk::Hint> SettingsScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Back"},
            tk::Hint{{tk::Action::PreviousTab, tk::Action::NextTab}, "Category"}};
}

std::unique_ptr<tk::Widget> SettingsSubPage::build(const tk::Theme& theme) {
    const ui::SettingsCategory& page = controller_.page(id());
    title_ = controller_.pageTitle(id());
    Frame frame = frameFor(theme, chrome_);
    tk::Column& side = frame.body->add<tk::Column>();
    side.fixWidth(theme.spacing.sidebarWidth);
    side.pad(tk::Insets{theme.spacing.gap, theme.spacing.pad, 0.0f, 0.0f});
    side.add<tk::Label>(title_, tk::TextRole::Title, true);
    addRows(*frame.body, theme, page, controller_);
    return std::move(frame.root);
}

bool SettingsSubPage::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    static_cast<void>(controller_.leave());
    return true;
}

} // namespace opensu::app
