#include "quick_settings_panel.hpp"

#include <algorithm>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr const char* chooseId = "quickPanel.choose";
/// The panel's width, in dp.
constexpr float panelDp = 360.0f;

} // namespace

const tk::Screen* QuickSettingsPanel::panel() const {
    const std::vector<const tk::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const tk::Screen* screen) {
        return dynamic_cast<const QuickSettingsScreen*>(screen) != nullptr;
    });
    return found == open.end() ? nullptr : *found;
}

bool QuickSettingsPanel::isOpen() const {
    return panel() != nullptr;
}

void QuickSettingsPanel::toggle() {
    if (isOpen()) {
        close();
        return;
    }
    // input-sound.md 3.4 OpenContextMenu: a menu becomes visible (r90.java).
    sounds_.play(audio::Effect::OpenContextMenu);
    settings_.read();
    navigator_.push(std::make_unique<QuickSettingsScreen>(*this));
}

void QuickSettingsPanel::close() {
    if (const tk::Screen* open = panel()) {
        // input-sound.md 3.4 Close: a menu becomes hidden.
        sounds_.play(audio::Effect::Close);
        navigator_.closeFrom(*open);
    }
}

std::vector<ui::SettingsRow> QuickSettingsPanel::rows() const {
    std::vector<ui::SettingsRow> rows = settings_.shown();
    rows.push_back(ui::makeRow(chooseId, ui::RowKind::Action, "Choose what shows", {}, {}));
    return rows;
}

void QuickSettingsPanel::run(const std::string& id, int step) {
    if (id == chooseId) {
        close();
        hooks_.choose();
        return;
    }
    settings_.run(id, step);
}

void QuickSettingsPanel::setLevel(const std::string& id, int level) {
    settings_.setLevel(id, level);
}

std::unique_ptr<tk::Widget> QuickSettingsScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Backdrop>();
    root->pad(tk::Insets::all(spacing.pad * 1.5f)).space(spacing.pad);
    root->add<tk::Label>(title(), tk::TextRole::Title);
    tk::ScrollView& scroll = root->add<tk::ScrollView>("quickSettings/rows");
    scroll.grow();
    tk::Column& side = scroll.content<tk::Column>();
    tk::Card& card = side.add<tk::Card>();
    card.pad(tk::Insets::all(spacing.gap * 2.0f)).space(spacing.gap);
    for (const ui::SettingsRow& row : panel_.rows()) {
        ui::addRow(card, row, panel_);
    }
    side.add<tk::Spacer>();
    root->add<tk::HintBar>();
    return root;
}

gpu2d::Rect QuickSettingsScreen::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    const float width = std::min(viewport.width, theme.px(panelDp + theme.spacing.pad * 3.0f));
    return gpu2d::Rect{viewport.x + viewport.width - width, viewport.y, width, viewport.height};
}

void QuickSettingsScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    tk::paintScrim(painter, viewport);
}

bool QuickSettingsScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    panel_.close();
    return true;
}

std::vector<tk::Hint> QuickSettingsScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

} // namespace opensu::app
