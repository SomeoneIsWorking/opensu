#include "game_settings_page.hpp"

#include <algorithm>
#include <utility>

#include "library/emulator_choice.hpp"
#include "library/rom_systems.hpp"
#include "retro_launch.hpp"
#include "settings/framegen_settings.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {
namespace {

using settings::Layer;
using ui::RowKind;
using ui::TabColumn;

constexpr std::string_view videoRowId = "tabs.video.";
constexpr std::string_view shaderRowId = "tabs.shader";
constexpr std::string_view frameGenRowId = "tabs.framegen.";
constexpr std::string_view frameGenHeadingId = "tabs.framegen";
constexpr std::string_view choiceRowId = "tabs.choice.";
constexpr std::string_view coreRowId = "tabs.core.";
constexpr std::string_view coreChoiceRowId = "tabs.coreChoice.";
constexpr std::string_view title = "Game settings";
constexpr std::string_view crumb = " › ";

ui::TabRow plain(ui::SettingsRow row) {
    return ui::TabRow{std::move(row), false, false};
}

/// The video or frame generation key `key` names, or null.
const settings::ChoiceKey* choiceKeyOf(std::string_view key) {
    const settings::ChoiceKey* video = settings::videoKey(key);
    return video != nullptr ? video : settings::frameGenKey(key);
}

/// The row of choice key `key`.
std::string rowIdOf(std::string_view key) {
    return std::string{settings::videoKey(key) != nullptr ? videoRowId : frameGenRowId} +
           std::string{key};
}

/// The key row `id` sets: the shader's, a video key's or a frame generation key's.
std::string keyOfRow(std::string_view id) {
    if (id == shaderRowId) {
        return std::string{settings::shaderKey};
    }
    if (id.starts_with(videoRowId)) {
        return std::string{id.substr(videoRowId.size())};
    }
    if (id.starts_with(frameGenRowId)) {
        return std::string{id.substr(frameGenRowId.size())};
    }
    if (id.starts_with(coreRowId)) {
        return std::string{id.substr(coreRowId.size())};
    }
    return {};
}

} // namespace

bool offersGameSettings(const library::Game& game, const library::roms::EmulatorCatalog& catalog,
                        bool frameGeneration) {
    return frameGeneration || builtInOf(game, catalog) != nullptr;
}

bool GameSettingsPage::offers(const library::Game& game) const {
    return offersGameSettings(game, downloads_.setup().catalog, frameGeneration_.available());
}

void GameSettingsPage::open(const library::Game& game, std::function<void()> back) {
    game_ = game;
    keys_ = library::layerKeysOf(game);
    video_ = builtInOf(game, downloads_.setup().catalog) != nullptr;
    frameGeneration_.findDll();
    coreOptions_.clear();
    if (retro_.status()) {
        coreOptions_ = retro_.options();
    }
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
    if (!coreOptions_.empty()) {
        shown.push_back(Tab::Core);
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
    case Tab::Core:
        return Layer{Layer::Kind::Emulator, keys_.emulator};
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
    case Tab::Core:
        return layerName(Layer{Layer::Kind::Emulator, keys_.emulator}, downloads_.setup().catalog);
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

std::string GameSettingsPage::valueAt(const Layer& layer, std::string_view key,
                                      const std::string& fallback) const {
    const settings::SettingLayers& layers = gameSettings_.layers();
    if (const std::string* own = layers.at(layer, key)) {
        return *own;
    }
    if (layer.kind == Layer::Kind::Global) {
        return fallback;
    }
    return layers.inherited(keys_, layer, key, fallback).value;
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
    } else if (settings::replacedByShader(key) &&
               !valueAt(layer, settings::shaderKey, {}).empty()) {
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
    if (tab_ == Tab::Core) {
        return coreRows();
    }
    std::vector<ui::TabRow> rows;
    if (video_) {
        for (const settings::ChoiceKey& video : settings::videoKeys) {
            rows.push_back(settingRow(rowIdOf(video.key), video.label, video.key,
                                      std::string{video.defaultKey()},
                                      [&video](const std::string& value) {
                                          return std::string{settings::choiceLabel(video, value)};
                                      }));
        }
        rows.push_back(settingRow(std::string{shaderRowId}, settings::shaderLabel,
                                  settings::shaderKey, std::string{}, [](const std::string& value) {
                                      return presetLabel(value);
                                  }));
    }
    if (frameGeneration_.available()) {
        addFrameGenRows(rows);
    }
    return rows;
}

std::vector<ui::TabRow> GameSettingsPage::coreRows() const {
    std::vector<ui::TabRow> rows;
    rows.reserve(coreOptions_.size());
    for (const RetroSession::CoreOption& option : coreOptions_) {
        rows.push_back(plain(ui::makeRow(std::string{coreRowId} + option.key, RowKind::Action,
                                         option.label, "", option.value)));
    }
    return rows;
}

void GameSettingsPage::addFrameGenRows(std::vector<ui::TabRow>& rows) const {
    const bool found = frameGeneration_.dll().has_value();
    rows.push_back(plain(ui::makeRow(
        std::string{frameGenHeadingId}, RowKind::Info, "Frame generation",
        found ? "Lossless Scaling, from Steam" : "Needs Lossless Scaling installed through Steam",
        "")));
    if (!found) {
        return;
    }
    const settings::ChoiceKey& modeKey = settings::frameGenKeys.front();
    const settings::FrameGenMode mode = settings::frameGenModeOf(
        valueAt(layerOf(tab_), modeKey.key, std::string{modeKey.defaultKey()}));
    for (const settings::ChoiceKey& key : settings::frameGenKeys) {
        if (!settings::frameGenKeyUsedIn(key.key, mode)) {
            continue;
        }
        rows.push_back(settingRow(rowIdOf(key.key), key.label, key.key,
                                  std::string{key.defaultKey()}, [&key](const std::string& value) {
                                      return std::string{settings::choiceLabel(key, value)};
                                  }));
    }
}

std::vector<ui::TabRow> GameSettingsPage::listRows() const {
    if (tab_ == Tab::Core) {
        std::vector<ui::TabRow> rows;
        const auto option =
            std::ranges::find(coreOptions_, listing_, &RetroSession::CoreOption::key);
        if (option == coreOptions_.end()) {
            return rows;
        }
        rows.reserve(option->values.size());
        for (const std::string& value : option->values) {
            rows.push_back(plain(ui::makeRow(std::string{coreChoiceRowId} + value, RowKind::Action,
                                             value, "", value == option->value ? "Chosen" : "")));
        }
        return rows;
    }
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
    const settings::ChoiceKey* listed = choiceKeyOf(listing_);
    if (listed == nullptr) {
        return rows;
    }
    const std::string current = valueAt(layer, listing_, std::string{listed->defaultKey()});
    for (const settings::ChoiceText& choice : listed->choices) {
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
    } else if (const settings::ChoiceKey* listed = choiceKeyOf(listing_)) {
        shown.title += std::string{crumb} + std::string{listed->label};
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
    if (tab_ == Tab::Core) {
        const std::string_view row{id};
        if (!row.starts_with(coreChoiceRowId)) {
            return;
        }
        const std::string back = std::string{coreRowId} + listing_;
        const std::string refused =
            retro_.setOption(listing_, std::string{row.substr(coreChoiceRowId.size())});
        if (!refused.empty()) {
            hooks_.say(refused, true);
        }
        coreOptions_ = retro_.options();
        listing_.clear();
        show();
        const std::vector<ui::TabRow>& rows = page_.view().rows;
        const auto at = std::ranges::find_if(rows, [&back](const ui::TabRow& each) {
            return each.row.id == back;
        });
        page_.focusRow(at == rows.end() ? 0 : static_cast<std::size_t>(at - rows.begin()),
                       TabColumn::Value);
        return;
    }
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
    const std::string back = rowIdOf(listing_);
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
    const bool shader = id == shaderRowId;
    const std::string key = keyOfRow(id);
    if (key.empty()) {
        return;
    }
    if (page_.column() == TabColumn::Override) {
        const settings::ChoiceKey* choice = choiceKeyOf(key);
        toggleOverride(key, choice != nullptr ? std::string{choice->defaultKey()} : std::string{});
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
            const std::string back = rowIdOf(listing_);
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
