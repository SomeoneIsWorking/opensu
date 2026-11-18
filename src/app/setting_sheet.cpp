#include "setting_sheet.hpp"

#include <algorithm>
#include <charconv>
#include <utility>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "shader_preset_tree.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;
using settings::Layer;
using settings::Setting;
using settings::SettingType;

std::string levelId(const Layer& level) {
    return "level/" + std::to_string(static_cast<int>(level.kind));
}

std::string removeId(const Layer& level) {
    return "remove/" + std::to_string(static_cast<int>(level.kind));
}

std::string addId(const Layer& level) {
    return "add/" + std::to_string(static_cast<int>(level.kind));
}

int levelOf(const std::string& value, const Setting::Range& range) {
    int level = range.low;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), level);
    if (error != std::errc{} || end != value.data() + value.size()) {
        return range.low;
    }
    return std::clamp(level, range.low, range.high);
}

} // namespace

std::string valueLabel(const Setting& setting, const std::string& value) {
    return setting.type == SettingType::Path ? presetLabel(value) : setting.labelOf(value);
}

SettingSheet::SettingSheet(LayeredSettings& settings, tk::Navigator& navigator,
                           const ui::Chrome& chrome, EditorContext context, EditorLook look,
                           std::string key, std::string label)
    : Screen{"setting/" + key, std::move(label)}, settings_{settings}, navigator_{navigator},
      chrome_{chrome}, context_{std::move(context)}, look_{look},
      schema_{settings_.schemaOf(context_)}, key_{std::move(key)} {
}

const settings::ScopedValue*
SettingSheet::find(const std::vector<settings::ScopedValue>& values) const {
    const auto at = std::ranges::find_if(values, [this](const settings::ScopedValue& value) {
        return value.setting->key == key_;
    });
    return at == values.end() ? nullptr : &*at;
}

const Layer& SettingSheet::layerOf(Layer::Kind kind) const {
    return *std::ranges::find(context_.scopes, kind, &Layer::kind);
}

std::string SettingSheet::levelName(const Layer& level) const {
    return level.kind == Layer::Kind::Game && !context_.gameTitle.empty()
               ? context_.gameTitle
               : settings_.scopeName(level);
}

std::string SettingSheet::levelTitle(const Layer& level) const {
    return level.kind == Layer::Kind::Global ? std::string{"All games"} : levelName(level);
}

std::unique_ptr<tk::Widget> SettingSheet::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(spacing.dialogWidth * 1.25f);
    // Clear of the top bar and the hint bar, which stay readable around the card.
    root->pad(
        tk::Insets{spacing.pad, chrome_.top / theme.scale, spacing.pad, spacing.hintBarHeight});
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Title, true);
    // Each level's value, least specific first.
    std::vector<std::pair<Layer, settings::ScopedValue>> levels;
    for (const Layer& level : context_.scopes) {
        const std::vector<settings::ScopedValue> values = settings_.rows(schema_, context_, level);
        if (const settings::ScopedValue* value = find(values)) {
            levels.emplace_back(level, *value);
        }
    }
    if (levels.empty()) {
        return root;
    }
    const settings::ScopedValue& shown = levels.back().second;
    const std::string from =
        shown.from ? " from " + levelName(*shown.from) : std::string{", the default"};
    card.add<tk::Label>(valueLabel(*shown.setting, shown.value) + from, tk::TextRole::Caption,
                        true);
    if (!shown.setting->help.empty()) {
        card.add<tk::Label>(shown.setting->help, tk::TextRole::Caption, true)
            .colour(theme.colours.inkMuted);
    }
    tk::List& list = card.add<tk::List>("levels", spacing.gap);
    list.grow();
    tk::Column& rows = list.rows();
    choiceKeys_.clear();
    for (const settings::SettingChoice& offered : shown.setting->choices) {
        choiceKeys_.push_back(offered.key);
    }
    focus_.clear();
    std::vector<std::pair<Layer, settings::ScopedValue>> open;
    for (const auto& [level, value] : levels) {
        if (level.kind == Layer::Kind::Global || value.own) {
            addLevel(rows, value, level, spacing.gap);
            focus_ = levelId(level);
        } else {
            open.emplace_back(level, value);
        }
    }
    if (!open.empty()) {
        rows.add<tk::SectionHeading>("Add override");
    }
    for (const auto& [level, value] : open) {
        addOverride(rows, value, level);
    }
    return root;
}

void SettingSheet::addLevel(tk::Column& rows, const settings::ScopedValue& value,
                            const Layer& level, float gap) {
    const Setting& setting = *value.setting;
    const std::string id = levelId(level);
    const Layer::Kind kind = level.kind;
    std::string subtitle;
    if (value.overriddenBy) {
        subtitle = levelName(*value.overriddenBy) + " uses " +
                   valueLabel(setting, value.overridingValue) + " over this";
    } else if (level.kind == Layer::Kind::Global) {
        subtitle = value.own ? std::string{"Every game, unless overridden below"}
                             : std::string{"The default, for every game"};
    } else {
        subtitle = "Override, in effect";
    }
    const tk::Item::Text text{levelTitle(level), std::move(subtitle)};
    // Beside the row, an × removes what the level keeps; X does it from the pad.
    tk::Container* line = &rows;
    if (value.own) {
        tk::Row& row = rows.add<tk::Row>();
        row.space(gap);
        line = &row;
    }
    tk::Item* item = nullptr;
    switch (setting.type) {
    case SettingType::Choice:
    case SettingType::Toggle:
    case SettingType::Preset: {
        std::vector<std::string> options;
        std::size_t chosen = 0;
        for (const settings::SettingChoice& offered : setting.choices) {
            if (offered.key == value.value) {
                chosen = options.size();
            }
            options.push_back(offered.label);
        }
        item = &line->add<tk::Choice>(
            id, text, std::move(options), chosen, [this, kind, chosen](std::size_t picked) {
                if (picked != chosen) {
                    settings_.set(context_, layerOf(kind), key_, choiceKeys_[picked]);
                }
            });
        break;
    }
    case SettingType::Slider: {
        auto& slider = line->add<tk::Slider>(
            id, text, tk::Slider::Range{setting.range.low, setting.range.high, setting.range.step},
            levelOf(value.value, setting.range), [this, kind](int picked) {
                settings_.set(context_, layerOf(kind), key_, std::to_string(picked));
            });
        slider.unit(setting.unit);
        item = &slider;
        break;
    }
    case SettingType::Path: {
        auto& path = line->add<tk::Item>(id, text);
        path.value(valueLabel(setting, value.value))
            .onPress(
                [this, kind] {
                    navigator_.push(std::make_unique<ShaderPresetScreen>(
                        settings_, navigator_, chrome_, context_, layerOf(kind), look_));
                },
                "Choose", true);
        item = &path;
        break;
    }
    }
    if (!value.own) {
        return;
    }
    item->grow();
    const auto remove = [this, kind] {
        const Layer& at = layerOf(kind);
        settings_.remove(at, key_);
        navigator_.focus(addId(at));
    };
    item->onAction(tk::Action::Secondary, remove, "Remove");
    line->add<tk::Button>(removeId(level), "\xC3\x97", remove);
}

void SettingSheet::addOverride(tk::Column& rows, const settings::ScopedValue& value,
                               const Layer& level) {
    const Setting& setting = *value.setting;
    const Layer::Kind kind = level.kind;
    const std::string* removed = settings_.removed(level, key_);
    const std::string now = valueLabel(setting, value.value);
    rows.add<tk::Item>(
            addId(level),
            tk::Item::Text{levelTitle(level), removed != nullptr
                                                  ? "Puts back " + valueLabel(setting, *removed)
                                                  : "Uses " + now + " now"})
        .onPress(
            [this, kind, restore = removed != nullptr, path = setting.type == SettingType::Path,
             start = value.value] {
                const Layer& at = layerOf(kind);
                if (restore) {
                    settings_.restore(at, key_);
                } else if (path) {
                    navigator_.push(std::make_unique<ShaderPresetScreen>(
                        settings_, navigator_, chrome_, context_, at, look_));
                    return;
                } else {
                    settings_.set(context_, at, key_, start);
                }
                navigator_.focus(levelId(at));
            },
            "Add", true);
}

void SettingSheet::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    tk::paintScrim(painter, viewport);
}

std::vector<tk::Hint> SettingSheet::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

std::string SettingSheet::initialFocus() const {
    return focus_;
}

} // namespace opensu::app
