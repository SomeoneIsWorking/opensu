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

std::string SettingSheet::levelName(const Layer& level) const {
    return level.kind == Layer::Kind::Game && !context_.gameTitle.empty()
               ? context_.gameTitle
               : settings_.scopeName(level);
}

std::string SettingSheet::levelTitle(const Layer& level) const {
    return level.kind == Layer::Kind::Global ? std::string{"Default for all games"}
                                             : "Override for " + levelName(level);
}

std::string SettingSheet::useText(const settings::ScopedValue& value) const {
    const std::string label = valueLabel(*value.setting, value.inherited.value);
    if (!value.inherited.from) {
        return "Use the default: " + label;
    }
    return "Use " + label + " from " + settings_.scopeName(*value.inherited.from);
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
    // The most specific level first: its value is the one in effect.
    std::vector<std::pair<Layer, settings::ScopedValue>> levels;
    for (auto level = context_.scopes.rbegin(); level != context_.scopes.rend(); ++level) {
        const std::vector<settings::ScopedValue> values = settings_.rows(schema_, context_, *level);
        if (const settings::ScopedValue* value = find(values)) {
            levels.emplace_back(*level, *value);
        }
    }
    if (!levels.empty()) {
        const settings::ScopedValue& shown = levels.front().second;
        const std::string from =
            shown.from ? " from " + levelName(*shown.from) : std::string{", the default"};
        card.add<tk::Label>(valueLabel(*shown.setting, shown.value) + from, tk::TextRole::Caption,
                            true);
        if (!shown.setting->help.empty()) {
            card.add<tk::Label>(shown.setting->help, tk::TextRole::Caption, true)
                .colour(theme.colours.inkMuted);
        }
    }
    tk::List& list = card.add<tk::List>("levels", spacing.gap);
    list.grow();
    tk::Column& rows = list.rows();
    std::vector<std::pair<Layer, settings::ScopedValue>> kept;
    for (const auto& [level, value] : levels) {
        addLevel(rows, value, level);
        if (value.own) {
            kept.emplace_back(level, value);
        }
    }
    if (!kept.empty()) {
        rows.add<tk::SectionHeading>("Remove");
    }
    for (const auto& [level, value] : kept) {
        const std::string name = level.kind == Layer::Kind::Global
                                     ? std::string{"Remove the default for all games"}
                                     : "Remove override for " + levelName(level);
        rows.add<tk::Item>(removeId(level), tk::Item::Text{name, useText(value)})
            .onPress([this, level = level] {
                settings_.remove(level, key_);
            });
    }
    return root;
}

void SettingSheet::addLevel(tk::Column& rows, const settings::ScopedValue& value,
                            const Layer& level) {
    const Setting& setting = *value.setting;
    const std::string id = levelId(level);
    std::string subtitle;
    if (value.overriddenBy) {
        subtitle = levelName(*value.overriddenBy) + " uses " +
                   valueLabel(setting, value.overridingValue) + " over this";
    } else if (value.own) {
        subtitle = "In effect";
    } else {
        subtitle = value.inherited.from ? "From " + settings_.scopeName(*value.inherited.from)
                                        : std::string{"The default"};
    }
    const tk::Item::Text text{levelTitle(level), std::move(subtitle)};
    tk::Item* row = nullptr;
    switch (setting.type) {
    case SettingType::Choice:
    case SettingType::Toggle:
    case SettingType::Preset: {
        // Place 0 uses what the level inherits; the inherited value is not offered again unless
        // the level keeps it.
        std::vector<std::string> options{valueLabel(setting, value.inherited.value)};
        std::vector<std::string> listed{useText(value)};
        std::vector<std::string> keys{std::string{}};
        std::size_t chosen = 0;
        for (const settings::SettingChoice& offered : setting.choices) {
            if (!value.own && offered.key == value.inherited.value) {
                continue;
            }
            if (value.own && offered.key == value.value) {
                chosen = keys.size();
            }
            options.push_back(offered.label);
            listed.push_back(offered.label);
            keys.push_back(offered.key);
        }
        auto& choice = rows.add<tk::Choice>(
            id, text, std::move(options), chosen,
            [this, level, keys = std::move(keys), own = value.own, chosen](std::size_t picked) {
                if (picked == chosen) {
                    return;
                }
                if (picked == 0) {
                    if (own) {
                        settings_.remove(level, key_);
                    }
                    return;
                }
                settings_.set(context_, level, key_, keys[picked]);
            });
        choice.listAs(std::move(listed));
        row = &choice;
        break;
    }
    case SettingType::Slider: {
        auto& slider = rows.add<tk::Slider>(
            id, text, tk::Slider::Range{setting.range.low, setting.range.high, setting.range.step},
            levelOf(value.value, setting.range), [this, level](int picked) {
                settings_.set(context_, level, key_, std::to_string(picked));
            });
        slider.unit(setting.unit);
        row = &slider;
        break;
    }
    case SettingType::Path: {
        auto& item = rows.add<tk::Item>(id, text);
        item.value(valueLabel(setting, value.value))
            .onPress(
                [this, level] {
                    navigator_.push(std::make_unique<ShaderPresetScreen>(
                        settings_, navigator_, chrome_, context_, level, look_));
                },
                "Choose", true);
        row = &item;
        break;
    }
    }
    if (const std::string* removed = !value.own ? settings_.removed(level, key_) : nullptr) {
        row->onAction(
            tk::Action::Tertiary,
            [this, level] {
                settings_.restore(level, key_);
            },
            "Restore " + valueLabel(setting, *removed));
    }
}

void SettingSheet::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    tk::paintScrim(painter, viewport);
}

std::vector<tk::Hint> SettingSheet::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

std::string SettingSheet::initialFocus() const {
    return levelId(context_.scopes.back());
}

} // namespace opensu::app
