#include "quick_settings_panel.hpp"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <utility>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "quick_setting_picker.hpp"
#include "ui/entry_screen.hpp"
#include "ui/icon_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The panel's width, in dp.
constexpr float panelDp = 360.0f;
/// The most bytes a category's name holds.
constexpr std::size_t longestCategoryName = 40;

constexpr std::string_view tabPrefix = "quick/tab/";
constexpr std::string_view categoryPrefix = "quick/category/";
constexpr std::string_view itemPrefix = "quick/item/";
constexpr std::string_view addPrefix = "quick/add/";
constexpr const char* addTabId = "quick/addTab";
constexpr const char* addCategoryId = "quick/addCategory";
constexpr const char* customizeId = "quick/customize";
constexpr const char* doneId = "quick/done";
constexpr const char* renameCardId = "quick-rename";
constexpr const char* gameNoneId = "quick/game";

std::string tabId(std::size_t tab) {
    return std::string{tabPrefix} + std::to_string(tab);
}

std::string categoryId(std::size_t category) {
    return std::string{categoryPrefix} + std::to_string(category);
}

std::string itemId(std::size_t category, std::size_t item) {
    return std::string{itemPrefix} + std::to_string(category) + "/" + std::to_string(item);
}

/// The numbers after `prefix` in `id`, split at '/'.
std::vector<std::size_t> numbersAfter(std::string_view id, std::string_view prefix) {
    std::vector<std::size_t> numbers;
    if (!id.starts_with(prefix)) {
        return numbers;
    }
    id.remove_prefix(prefix.size());
    while (!id.empty()) {
        const std::size_t slash = id.find('/');
        const std::string_view part = id.substr(0, slash);
        std::size_t value = 0;
        for (const char digit : part) {
            if (digit < '0' || digit > '9') {
                return {};
            }
            value = value * 10 + static_cast<std::size_t>(digit - '0');
        }
        numbers.push_back(value);
        if (slash == std::string_view::npos) {
            break;
        }
        id.remove_prefix(slash + 1);
    }
    return numbers;
}

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
    open(0, false);
}

void QuickSettingsPanel::open(std::size_t tab, bool customize) {
    close();
    game_.reset();
    // input-sound.md 3.4 OpenContextMenu: a menu becomes visible (r90.java).
    sounds_.play(audio::Effect::OpenContextMenu);
    system_.read();
    settings_.refresh();
    navigator_.push(std::make_unique<QuickSettingsScreen>(*this, tab, customize));
}

void QuickSettingsPanel::openGame() {
    open(layout_.gameTab(), false);
}

const EditorPages* QuickSettingsPanel::gamePages() {
    const library::Game* game = hooks_.game ? hooks_.game() : nullptr;
    if (game == nullptr) {
        game_.reset();
        return nullptr;
    }
    if (!game_ || game_->context.gameId != game->id) {
        game_ = std::make_unique<GamePages>(layered_, navigator_, chrome_, layered_.forGame(*game));
    }
    return &game_->pages;
}

std::string QuickSettingsPanel::gameTitle() const {
    return game_ ? game_->context.gameTitle : std::string{};
}

void QuickSettingsPanel::close() {
    if (const tk::Screen* open = panel()) {
        // input-sound.md 3.4 Close: a menu becomes hidden.
        sounds_.play(audio::Effect::Close);
        navigator_.closeFrom(*open);
    }
}

std::optional<ui::SettingsRow> QuickSettingsPanel::rowOf(const settings::QuickPin& pin) {
    const ui::SettingsCategory& page = settings_.page(pin.page);
    const auto found = std::ranges::find(page.rows, pin.row, &ui::SettingsRow::id);
    if (found == page.rows.end() || !QuickLayoutEditor::pinnable(*found)) {
        return std::nullopt;
    }
    return *found;
}

void QuickSettingsPanel::rename(std::size_t tab, std::size_t category) {
    const settings::QuickLayout now = layout_.layout();
    if (tab >= now.tabs.size() || category >= now.tabs[tab].categories.size()) {
        return;
    }
    namingTab_ = tab;
    namingCategory_ = category;
    draft_ = now.tabs[tab].categories[category].name;
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = renameCardId,
                  .title = "Rename " + draft_,
                  .note = "The heading over this category's settings.",
                  .label = "Name",
                  .placeholder = draft_,
                  .secret = false,
                  .mostBytes = longestCategoryName,
                  .text = [this]() -> const std::string& {
                      return draft_;
                  },
                  .changed =
                      [this](std::string text) {
                          draft_ = std::move(text);
                      },
                  .submitted =
                      [this] {
                          closeEntry();
                          if (!draft_.empty()) {
                              layout_.renameCategory(namingTab_, namingCategory_, draft_);
                          }
                      },
                  .cancelled =
                      [this] {
                          closeEntry();
                      }}));
    keyboard_.open();
}

void QuickSettingsPanel::closeEntry() {
    for (const tk::Screen* screen : navigator_.screens()) {
        if (screen->id() == renameCardId) {
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

void QuickSettingsPanel::chooseIcon(std::size_t tab) {
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<QuickIconScreen>(*this, tab));
}

void QuickSettingsPanel::addSetting(std::size_t tab, std::size_t category) {
    sounds_.play(audio::Effect::Open);
    settings_.refresh();
    navigator_.push(std::make_unique<QuickSettingPicker>(*this, tab, category));
}

void QuickSettingsPanel::say(const std::string& text) const {
    hooks_.say(text, false);
}

void QuickSettingsPanel::run(const std::string& id, int step) {
    settings_.run(id, step);
    settings_.refresh();
}

void QuickSettingsPanel::setLevel(const std::string& id, int level) {
    settings_.setLevel(id, level);
    settings_.refresh();
}

std::size_t QuickSettingsScreen::tabCount() const {
    return panel_.layout().layout().tabs.size();
}

std::unique_ptr<tk::Widget> QuickSettingsScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    const settings::QuickLayout layout = panel_.layout().layout();
    if (!layout.tabs.empty()) {
        tab_ = std::min(tab_, layout.tabs.size() - 1);
    }
    auto root = std::make_unique<tk::Backdrop>();
    root->pad(tk::Insets::all(spacing.pad * 1.5f)).space(spacing.pad);
    root->add<tk::Label>(editing_ ? std::string{"Customize"} : title(), tk::TextRole::Title);
    buildTabs(*root, theme, layout);
    tk::ScrollView& scroll = root->add<tk::ScrollView>("quickSettings/rows");
    scroll.grow();
    tk::Column& side = scroll.content<tk::Column>();
    side.space(spacing.pad);
    if (editing_) {
        buildEdit(side, theme, layout.tabs.empty() ? settings::QuickTab{} : layout.tabs[tab_]);
    } else if (!layout.tabs.empty()) {
        buildUse(side, theme, layout.tabs[tab_]);
    } else {
        side.add<tk::Item>(customizeId, tk::Item::Text{"Customize", "Add a tab and its settings"})
            .onPress([this] {
                editing_ = true;
            });
    }
    side.add<tk::Spacer>();
    root->add<tk::HintBar>();
    return root;
}

void QuickSettingsScreen::buildTabs(tk::Column& root, const tk::Theme& theme,
                                    const settings::QuickLayout& layout) {
    tk::Row& tabs = root.add<tk::Row>();
    tabs.space(theme.spacing.gap);
    for (std::size_t at = 0; at < layout.tabs.size(); ++at) {
        const ui::Icon icon = iconOf(quickIconOf(layout.tabs[at].icon));
        tabs.add<ui::IconTab>(
            tabId(at), editing_ ? (grab_ ? "Drop" : "Move") : "Open",
            [this, at] {
                if (editing_) {
                    tab_ = at;
                    press(Grab::Kind::Tab, ItemPlace{at, 0, 0});
                    return;
                }
                tab_ = at;
            },
            at == tab_, panel_.images(), icon);
    }
    if (editing_ && !grab_) {
        tabs.add<ui::IconTab>(
            addTabId, "Add a tab",
            [this] {
                tab_ = panel_.layout().addTab(QuickIcon::Star);
            },
            false, panel_.images(), ui::Icon::QuickPlus);
    }
}

void QuickSettingsScreen::buildGame(tk::Column& rows, const tk::Theme& theme) {
    firstGameRow_.clear();
    const EditorPages* pages = panel_.gamePages();
    if (pages == nullptr) {
        rows.add<tk::Item>(gameNoneId, tk::Item::Text{"No game is running",
                                                      "The running game's settings show here"});
        return;
    }
    rows.add<tk::SectionHeading>(panel_.gameTitle());
    tk::Card& card = rows.add<tk::Card>();
    card.pad(tk::Insets::all(theme.spacing.gap * 2.0f)).space(theme.spacing.gap);
    firstGameRow_ = pages->addCategories(card, pages->values());
}

void QuickSettingsScreen::buildUse(tk::Column& rows, const tk::Theme& theme,
                                   const settings::QuickTab& tab) {
    const tk::Spacing& spacing = theme.spacing;
    if (tab.game) {
        buildGame(rows, theme);
    }
    for (const settings::QuickCategory& category : tab.categories) {
        std::vector<ui::SettingsRow> shown;
        for (const settings::QuickPin& pin : category.items) {
            if (std::optional<ui::SettingsRow> row = panel_.rowOf(pin)) {
                shown.push_back(std::move(*row));
            }
        }
        if (shown.empty()) {
            continue;
        }
        rows.add<tk::SectionHeading>(category.name);
        tk::Card& card = rows.add<tk::Card>();
        card.pad(tk::Insets::all(spacing.gap * 2.0f)).space(spacing.gap);
        for (const ui::SettingsRow& row : shown) {
            ui::addRow(card, row, panel_);
        }
    }
    rows.add<tk::Item>(customizeId, tk::Item::Text{"Customize", {}}).onPress([this] {
        editing_ = true;
    });
}

void QuickSettingsScreen::buildEdit(tk::Column& rows, const tk::Theme& theme,
                                    const settings::QuickTab& tab) {
    const tk::Spacing& spacing = theme.spacing;
    const auto held = [this](Grab::Kind kind, ItemPlace place) {
        return grab_ && grab_->kind == kind && grab_->place == place;
    };
    if (tab.game) {
        rows.add<tk::Item>(gameNoneId,
                           tk::Item::Text{"The running game's settings",
                                          "Shown here while a game runs; the tab moves but stays"});
        if (!grab_) {
            rows.add<tk::Item>(doneId, tk::Item::Text{"Done", {}}).onPress([this] {
                editing_ = false;
            });
        }
        return;
    }
    for (std::size_t c = 0; c < tab.categories.size(); ++c) {
        const settings::QuickCategory& category = tab.categories[c];
        const ItemPlace categoryPlace{tab_, c, 0};
        tk::Card& card = rows.add<tk::Card>();
        card.pad(tk::Insets::all(spacing.gap * 2.0f)).space(spacing.gap);
        card.add<tk::Item>(categoryId(c), tk::Item::Text{category.name, "Category"})
            .value(held(Grab::Kind::Category, categoryPlace) ? "Moving" : "")
            .onPress(
                [this, categoryPlace] {
                    press(Grab::Kind::Category, categoryPlace);
                },
                grab_ ? "Drop" : "Move");
        for (std::size_t i = 0; i < category.items.size(); ++i) {
            const settings::QuickPin& pin = category.items[i];
            const std::optional<ui::SettingsRow> row = panel_.rowOf(pin);
            const ItemPlace place{tab_, c, i};
            const std::string page = panel_.settings().pageTitle(pin.page);
            std::string caption = page;
            if (!row) {
                caption = page.empty() ? "Not on this machine" : page + " · not on this machine";
            }
            card.add<tk::Item>(itemId(c, i),
                               tk::Item::Text{row ? row->label : pin.label, std::move(caption)})
                .value(held(Grab::Kind::Item, place) ? "Moving" : "")
                .onPress(
                    [this, place] {
                        press(Grab::Kind::Item, place);
                    },
                    grab_ ? "Drop" : "Move");
        }
        if (!grab_) {
            card.add<tk::Item>(std::string{addPrefix} + std::to_string(c),
                               tk::Item::Text{"Add a setting", {}})
                .onPress(
                    [this, c] {
                        panel_.addSetting(tab_, c);
                    },
                    "Add", true);
        }
    }
    if (grab_) {
        return;
    }
    if (tabCount() > 0) {
        rows.add<tk::Item>(addCategoryId, tk::Item::Text{"Add a category", {}}).onPress([this] {
            const std::size_t added = panel_.layout().addCategory(tab_, {});
            panel_.rename(tab_, added);
        });
    }
    rows.add<tk::Item>(doneId, tk::Item::Text{"Done", {}}).onPress([this] {
        editing_ = false;
    });
}

void QuickSettingsScreen::press(Grab::Kind kind, ItemPlace place) {
    QuickLayoutEditor& layout = panel_.layout();
    if (!grab_) {
        grab_ = Grab{kind, place, layout.layout()};
        return;
    }
    // A tap on another row of the same kind drops what is held there.
    if (grab_->kind == kind && !(grab_->place == place)) {
        while (!(grab_->place == place)) {
            const ItemPlace was = grab_->place;
            const bool down = std::tie(place.category, place.item) >
                              std::tie(grab_->place.category, grab_->place.item);
            switch (kind) {
            case Grab::Kind::Item:
                grab_->place = layout.moveItem(grab_->place, down ? 1 : -1);
                break;
            case Grab::Kind::Category:
                grab_->place.category =
                    layout.moveCategory(grab_->place.tab, grab_->place.category, down ? 1 : -1);
                break;
            case Grab::Kind::Tab:
                grab_->place.tab = layout.moveTab(grab_->place.tab, place.tab > was.tab ? 1 : -1);
                tab_ = grab_->place.tab;
                break;
            }
            if (grab_->place == was) {
                break;
            }
        }
    }
    grab_.reset();
}

void QuickSettingsScreen::move(int step, tk::ActContext& context) {
    QuickLayoutEditor& layout = panel_.layout();
    switch (grab_->kind) {
    case Grab::Kind::Item:
        grab_->place = layout.moveItem(grab_->place, step);
        context.focus(itemId(grab_->place.category, grab_->place.item));
        break;
    case Grab::Kind::Category:
        grab_->place.category = layout.moveCategory(grab_->place.tab, grab_->place.category, step);
        context.focus(categoryId(grab_->place.category));
        break;
    case Grab::Kind::Tab:
        grab_->place.tab = layout.moveTab(grab_->place.tab, step);
        tab_ = grab_->place.tab;
        context.focus(tabId(grab_->place.tab));
        break;
    }
}

std::optional<std::pair<QuickSettingsScreen::Grab::Kind, ItemPlace>>
QuickSettingsScreen::focusedPlace() const {
    const std::string& focused = view().focused;
    if (const std::vector<std::size_t> n = numbersAfter(focused, tabPrefix); n.size() == 1) {
        return std::pair{Grab::Kind::Tab, ItemPlace{n[0], 0, 0}};
    }
    if (const std::vector<std::size_t> n = numbersAfter(focused, categoryPrefix); n.size() == 1) {
        return std::pair{Grab::Kind::Category, ItemPlace{tab_, n[0], 0}};
    }
    if (const std::vector<std::size_t> n = numbersAfter(focused, itemPrefix); n.size() == 2) {
        return std::pair{Grab::Kind::Item, ItemPlace{tab_, n[0], n[1]}};
    }
    return std::nullopt;
}

void QuickSettingsScreen::removeFocused() {
    const auto focused = focusedPlace();
    if (!focused) {
        return;
    }
    QuickLayoutEditor& layout = panel_.layout();
    const auto [kind, place] = *focused;
    switch (kind) {
    case Grab::Kind::Item:
        layout.removeItem(place);
        break;
    case Grab::Kind::Category:
        layout.removeCategory(place.tab, place.category);
        break;
    case Grab::Kind::Tab:
        if (!layout.removeTab(place.tab)) {
            panel_.say("The Game tab stays: Game settings in the Guide opens it");
            break;
        }
        tab_ = place.tab == 0 ? 0 : place.tab - 1;
        break;
    }
}

std::string QuickSettingsScreen::initialFocus() const {
    const settings::QuickLayout layout = panel_.layout().layout();
    if (layout.tabs.empty()) {
        return customizeId;
    }
    if (layout.tabs[tab_].game) {
        return firstGameRow_.empty() ? tabId(tab_) : firstGameRow_;
    }
    for (const settings::QuickCategory& category : layout.tabs[tab_].categories) {
        for (const settings::QuickPin& pin : category.items) {
            if (const std::optional<ui::SettingsRow> row = panel_.rowOf(pin)) {
                return row->id;
            }
        }
    }
    return tabId(tab_);
}

gpu2d::Rect QuickSettingsScreen::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    const float width = std::min(viewport.width, theme.px(panelDp + theme.spacing.pad * 3.0f));
    return gpu2d::Rect{viewport.x + viewport.width - width, viewport.y, width, viewport.height};
}

void QuickSettingsScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    tk::paintScrim(painter, viewport);
}

tk::Screen::BackReply QuickSettingsScreen::back(tk::ActContext& /*context*/) {
    if (grab_) {
        panel_.layout().keep(grab_->before);
        grab_.reset();
        return BackReply::Stay;
    }
    if (editing_) {
        editing_ = false;
        return BackReply::Stay;
    }
    return BackReply::Pop;
}

bool QuickSettingsScreen::act(tk::Action action, tk::ActContext& context) {
    if (action == tk::Action::Menu) {
        panel_.close();
        return true;
    }
    const bool tabs = action == tk::Action::PreviousTab || action == tk::Action::NextTab;
    const int step =
        action == tk::Action::PreviousTab || action == tk::Action::Up || action == tk::Action::Left
            ? -1
            : 1;
    if (grab_) {
        const bool along = grab_->kind == Grab::Kind::Tab
                               ? tabs || action == tk::Action::Left || action == tk::Action::Right
                               : action == tk::Action::Up || action == tk::Action::Down;
        if (along) {
            move(step, context);
        }
        // Nothing else moves the focus while something is held.
        return along || tabs || action == tk::Action::Up || action == tk::Action::Down ||
               action == tk::Action::Left || action == tk::Action::Right;
    }
    if (tabs) {
        const std::size_t count = tabCount();
        if (count > 0) {
            tab_ = step < 0 ? (tab_ + count - 1) % count : (tab_ + 1) % count;
        }
        if (const auto focused = focusedPlace(); focused && focused->first == Grab::Kind::Tab) {
            context.focus(tabId(tab_));
        }
        return true;
    }
    if (action == tk::Action::Options) {
        editing_ = !editing_;
        return true;
    }
    if (!editing_) {
        return false;
    }
    if (action == tk::Action::Secondary) {
        removeFocused();
        return true;
    }
    if (action == tk::Action::Tertiary) {
        const auto focused = focusedPlace();
        if (focused && focused->first == Grab::Kind::Category) {
            panel_.rename(tab_, focused->second.category);
        } else if (focused && focused->first == Grab::Kind::Tab) {
            panel_.chooseIcon(focused->second.tab);
        }
        return focused.has_value();
    }
    return false;
}

std::vector<tk::Hint> QuickSettingsScreen::hints() const {
    if (grab_) {
        return {tk::Hint{{tk::Action::Accept}, "Drop"}, tk::Hint{{tk::Action::Back}, "Put back"}};
    }
    if (!editing_) {
        return {tk::Hint{{tk::Action::PreviousTab, tk::Action::NextTab}, "Tab"},
                tk::Hint{{tk::Action::Options}, "Customize"},
                tk::Hint{{tk::Action::Back}, "Close"}};
    }
    std::vector<tk::Hint> hints;
    const auto focused = focusedPlace();
    if (focused) {
        hints.push_back(tk::Hint{{tk::Action::Accept}, "Move"});
        const bool stays =
            focused->first == Grab::Kind::Tab && focused->second.tab == panel_.layout().gameTab();
        if (!stays) {
            hints.push_back(tk::Hint{{tk::Action::Secondary}, "Remove"});
        }
        if (focused->first == Grab::Kind::Category) {
            hints.push_back(tk::Hint{{tk::Action::Tertiary}, "Rename"});
        } else if (focused->first == Grab::Kind::Tab) {
            hints.push_back(tk::Hint{{tk::Action::Tertiary}, "Icon"});
        }
    }
    hints.push_back(tk::Hint{{tk::Action::Back}, "Done"});
    return hints;
}

} // namespace opensu::app
