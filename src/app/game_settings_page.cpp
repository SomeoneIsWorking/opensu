#include "game_settings_page.hpp"

#include <algorithm>
#include <utility>

#include "library/emulator_choice.hpp"
#include "library/rom_systems.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {
namespace {

using settings::Layer;
using ui::RowKind;
using ui::TabColumn;

constexpr std::string_view videoRowId = "tabs.video.";
constexpr std::string_view shaderRowId = "tabs.shader";
constexpr std::string_view choiceRowId = "tabs.choice.";
constexpr std::string_view title = "Game settings";
constexpr std::string_view crumb = " › ";

ui::TabRow plain(ui::SettingsRow row) {
    return ui::TabRow{std::move(row), false, false};
}

} // namespace

void GameSettingsPage::open(const library::Game& game, std::function<void()> back) {
    game_ = game;
    keys_ = library::layerKeysOf(game);
    back_ = std::move(back);
    tab_ = Tab::Game;
    listing_.clear();
    picker_.close();
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    page_.open(view());
}

void GameSettingsPage::close() noexcept {
    page_.close();
    picker_.close();
    listing_.clear();
    back_ = nullptr;
}

std::vector<GameSettingsPage::Tab> GameSettingsPage::tabs() const {
    std::vector<Tab> shown{Tab::Global};
    if (!keys_.system.empty()) {
        shown.push_back(Tab::System);
    }
    shown.push_back(Tab::Game);
    return shown;
}

Layer GameSettingsPage::layerOf(Tab tab) const {
    switch (tab) {
    case Tab::Global:
        break;
    case Tab::System:
        return Layer{Layer::Kind::System, keys_.system};
    case Tab::Game:
        return Layer{Layer::Kind::Game, keys_.game};
    }
    return Layer{};
}

std::string GameSettingsPage::tabName() const {
    return isOpen() ? labelOf(tab_) : std::string{};
}

std::string GameSettingsPage::labelOf(Tab tab) const {
    switch (tab) {
    case Tab::Global:
        return "Global";
    case Tab::System:
        return library::roms::systemLabel(keys_.system);
    case Tab::Game:
        break;
    }
    return "Game";
}

std::string GameSettingsPage::focusName() const {
    const ui::TabRow* row = page_.focusedRow();
    if (!isOpen() || row == nullptr) {
        return {};
    }
    return row->row.id + (page_.column() == TabColumn::Override ? ":override" : ":value");
}

std::string GameSettingsPage::shaderAt(const Layer& layer) const {
    const settings::SettingLayers& layers = gameSettings_.layers();
    if (const std::string* own = layers.at(layer, settings::shaderKey)) {
        return *own;
    }
    if (layer.kind == Layer::Kind::Global) {
        return {};
    }
    return layers.inherited(keys_, layer, settings::shaderKey, std::string{}).value;
}

ui::TabRow
GameSettingsPage::settingRow(const std::string& id, std::string_view label, std::string_view key,
                             const std::string& fallback,
                             const std::function<std::string(const std::string&)>& shownAs) const {
    const settings::SettingLayers& layers = gameSettings_.layers();
    const library::roms::EmulatorCatalog& catalog = downloads_.setup().catalog;
    const Layer layer = layerOf(tab_);
    const bool global = tab_ == Tab::Global;
    const std::string* own = layers.at(layer, key);
    const settings::Resolved inherited = global ? settings::Resolved{fallback, std::nullopt}
                                                : layers.inherited(keys_, layer, key, fallback);
    std::string note;
    if (const std::optional<Layer> over = layers.over(keys_, layer, key)) {
        note = "Overridden by " + layerName(*over, catalog);
    } else if (settings::replacedByShader(key) && !shaderAt(layer).empty()) {
        note = "Not used with a shader preset";
    } else if (own != nullptr) {
        note = global ? std::string{} : "Set for " + layerName(layer, catalog);
    } else {
        note = sourceText(inherited.from, catalog);
    }
    return ui::TabRow{ui::makeRow(id, RowKind::Action, std::string{label}, std::move(note),
                                  shownAs(own != nullptr ? *own : inherited.value)),
                      !global, own != nullptr};
}

std::vector<ui::TabRow> GameSettingsPage::tabRows() const {
    std::vector<ui::TabRow> rows;
    rows.reserve(settings::videoKeys.size() + 1);
    for (const settings::VideoKey& video : settings::videoKeys) {
        rows.push_back(settingRow(std::string{videoRowId} + std::string{video.key}, video.label,
                                  video.key, std::string{video.choices.front().key},
                                  [&video](const std::string& value) {
                                      return std::string{settings::choiceLabel(video, value)};
                                  }));
    }
    rows.push_back(settingRow(std::string{shaderRowId}, settings::shaderLabel, settings::shaderKey,
                              std::string{}, [](const std::string& value) {
                                  return presetLabel(value);
                              }));
    return rows;
}

std::vector<ui::TabRow> GameSettingsPage::listRows() const {
    const settings::SettingLayers& layers = gameSettings_.layers();
    const Layer layer = layerOf(tab_);
    std::vector<ui::TabRow> rows;
    if (picker_.isOpen()) {
        for (ui::SettingsRow& row : picker_.rows(
                 layers.at(layer, settings::shaderKey),
                 tab_ == Tab::Global
                     ? std::string{}
                     : layers.inherited(keys_, layer, settings::shaderKey, std::string{}).value)) {
            rows.push_back(plain(std::move(row)));
        }
        return rows;
    }
    const settings::VideoKey* video = settings::videoKey(listing_);
    if (video == nullptr) {
        return rows;
    }
    const std::string* own = layers.at(layer, listing_);
    const std::string current =
        own != nullptr
            ? *own
            : layers.inherited(keys_, layer, listing_, std::string{video->choices[0].key}).value;
    for (const settings::ChoiceText& choice : video->choices) {
        rows.push_back(plain(ui::makeRow(std::string{choiceRowId} + std::string{choice.key},
                                         RowKind::Action, std::string{choice.label}, "",
                                         choice.key == current ? "Chosen" : "")));
    }
    return rows;
}

ui::TabsView GameSettingsPage::view() const {
    ui::TabsView shown;
    shown.title = std::string{title};
    if (picker_.isOpen()) {
        shown.title += std::string{crumb} + picker_.openName();
    } else if (const settings::VideoKey* video = settings::videoKey(listing_)) {
        shown.title += std::string{crumb} + std::string{video->label};
    }
    const std::vector<Tab> order = tabs();
    for (const Tab tab : order) {
        shown.tabs.push_back(labelOf(tab));
    }
    shown.tab = static_cast<std::size_t>(std::ranges::find(order, tab_) - order.begin());
    shown.rows = picker_.isOpen() || !listing_.empty() ? listRows() : tabRows();
    return shown;
}

void GameSettingsPage::show() {
    page_.refresh(view());
}

void GameSettingsPage::showTab(std::size_t index) {
    const std::vector<Tab> order = tabs();
    if (index >= order.size() || order[index] == tab_) {
        return;
    }
    // input-sound.md 3.4 Navigation: the page in view changes.
    sounds_.play(audio::Effect::Navigation);
    picker_.close();
    listing_.clear();
    tab_ = order[index];
    show();
}

void GameSettingsPage::changed() {
    gameSettings_.save();
    if (game_) {
        hooks_.changed(*game_);
    }
    show();
}

void GameSettingsPage::toggleOverride(std::string_view key, const std::string& fallback) {
    const Layer layer = layerOf(tab_);
    settings::SettingLayers& layers = gameSettings_.layers();
    if (layers.at(layer, key) != nullptr) {
        layers.clear(layer, key);
    } else {
        layers.set(layer, key, layers.inherited(keys_, layer, key, fallback).value);
    }
    changed();
}

void GameSettingsPage::pick(const std::string& id) {
    const Layer layer = layerOf(tab_);
    settings::SettingLayers& layers = gameSettings_.layers();
    if (picker_.isOpen()) {
        const ShaderPresetPicker::Pick pick = picker_.press(id);
        using Kind = ShaderPresetPicker::Pick::Kind;
        if (pick.kind == Kind::Nothing) {
            show();
            page_.focusRow(0, TabColumn::Value);
            return;
        }
        if (pick.kind == Kind::Clear) {
            layers.clear(layer, settings::shaderKey);
        } else {
            layers.set(layer, settings::shaderKey, pick.value);
        }
        picker_.close();
        changed();
        return;
    }
    const std::string_view row{id};
    if (!row.starts_with(choiceRowId)) {
        return;
    }
    layers.set(layer, listing_, std::string{row.substr(choiceRowId.size())});
    const std::string back = std::string{videoRowId} + listing_;
    listing_.clear();
    changed();
    const std::vector<ui::TabRow>& rows = page_.view().rows;
    const auto at = std::ranges::find_if(rows, [&back](const ui::TabRow& each) {
        return each.row.id == back;
    });
    if (at != rows.end()) {
        page_.focusRow(static_cast<std::size_t>(at - rows.begin()), TabColumn::Value);
    }
}

void GameSettingsPage::press() {
    const ui::TabRow* focused = page_.focusedRow();
    if (focused == nullptr || focused->row.kind == RowKind::Info) {
        return;
    }
    const std::string id = focused->row.id;
    sounds_.play(audio::Effect::Navigation);
    if (picker_.isOpen() || !listing_.empty()) {
        pick(id);
        return;
    }
    const std::string_view row{id};
    const bool shader = row == shaderRowId;
    const std::string key =
        shader ? std::string{settings::shaderKey} : std::string{row.substr(videoRowId.size())};
    if (page_.column() == TabColumn::Override) {
        const settings::VideoKey* video = settings::videoKey(key);
        toggleOverride(key, video != nullptr ? std::string{video->choices[0].key} : std::string{});
        return;
    }
    if (shader) {
        picker_.open();
    } else {
        listing_ = key;
    }
    show();
    const std::vector<ui::TabRow>& rows = page_.view().rows;
    const auto chosen = std::ranges::find_if(rows, [](const ui::TabRow& each) {
        return each.row.value == "Chosen";
    });
    page_.focusRow(chosen == rows.end() ? 0 : static_cast<std::size_t>(chosen - rows.begin()),
                   TabColumn::Value);
}

void GameSettingsPage::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move in a list.
        if (page_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right:
        if (page_.moveColumn(button == gamepad::Button::Left ? TabColumn::Override
                                                             : TabColumn::Value)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::L1:
    case gamepad::Button::R1: {
        const std::vector<Tab> order = tabs();
        const auto at = static_cast<std::size_t>(std::ranges::find(order, tab_) - order.begin());
        if (button == gamepad::Button::L1 && at > 0) {
            showTab(at - 1);
        } else if (button == gamepad::Button::R1) {
            showTab(at + 1);
        }
        break;
    }
    case gamepad::Button::A:
        press();
        break;
    case gamepad::Button::B: {
        if (picker_.isOpen()) {
            const bool within = picker_.back();
            show();
            if (!within) {
                const std::vector<ui::TabRow>& rows = page_.view().rows;
                page_.focusRow(rows.empty() ? 0 : rows.size() - 1, TabColumn::Value);
            }
            break;
        }
        if (!listing_.empty()) {
            const std::string back = std::string{videoRowId} + listing_;
            listing_.clear();
            show();
            const std::vector<ui::TabRow>& rows = page_.view().rows;
            const auto at = std::ranges::find_if(rows, [&back](const ui::TabRow& each) {
                return each.row.id == back;
            });
            page_.focusRow(at == rows.end() ? 0 : static_cast<std::size_t>(at - rows.begin()),
                           TabColumn::Value);
            break;
        }
        // input-sound.md 3.4 Close: the player dismisses a panel.
        sounds_.play(audio::Effect::Close);
        std::function<void()> back = std::move(back_);
        close();
        if (back) {
            back();
        }
        break;
    }
    default:
        break;
    }
}

} // namespace opensu::app
