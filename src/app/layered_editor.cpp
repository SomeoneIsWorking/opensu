#include "layered_editor.hpp"

#include <algorithm>
#include <charconv>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "settings/framegen_settings.hpp"
#include "settings/scaling_settings.hpp"
#include "shader_preset_tree.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;
using settings::Layer;
using settings::Setting;
using settings::SettingType;

constexpr std::string_view rowPrefix = "row/";
constexpr std::string_view scopesId = "scopes";
constexpr std::string_view clearId = "clear";
/// The panel's share of the window's width, and the most it takes.
constexpr float panelShare = 0.5f;
constexpr float panelMaxDp = 720.0f;

/// `scope` as a sentence names it: "this game", "Game Boy Advance".
std::string phrased(const LayeredSettings& settings, const Layer& scope) {
    return scope.kind == Layer::Kind::Game ? std::string{"this game"} : settings.scopeName(scope);
}

std::string rowId(std::string_view key) {
    return std::string{rowPrefix} + std::string{key};
}

std::string countOf(std::size_t count, std::string_view noun) {
    return std::to_string(count) + " " + std::string{noun} + (count == 1 ? "" : "s");
}

/// The tree a screen of `look` lays out in: the side column (a page's) or the panel's head, the
/// column its body goes in, and the hint bar.
struct Frame {
    std::unique_ptr<tk::Widget> root;
    tk::Column* head{};
    tk::Column* body{};
};

Frame frameFor(EditorLook look, const tk::Theme& theme, const ui::Chrome& chrome) {
    const float pad = theme.spacing.pad;
    if (look == EditorLook::Panel) {
        auto card = std::make_unique<tk::Card>();
        card->pad(tk::Insets::all(pad)).space(theme.spacing.gap * 2.0f);
        tk::Column& head = card->add<tk::Column>();
        tk::Column& body = card->add<tk::Column>();
        body.grow();
        body.space(theme.spacing.gap * 2.0f);
        card->add<tk::HintBar>().fixHeight(theme.spacing.hintBarHeight);
        tk::Column* bodyAt = &body;
        tk::Column* headAt = &head;
        return Frame{std::move(card), headAt, bodyAt};
    }
    auto root = std::make_unique<tk::Backdrop>();
    root->space(theme.spacing.gap);
    root->add<tk::Spacer>(0.0f).fixHeight(chrome.top / theme.scale);
    tk::Row& row = root->add<tk::Row>();
    row.align(tk::Align::Stretch);
    row.pad(tk::Insets{pad * 2.0f, 0.0f, pad * 2.0f, 0.0f}).space(pad);
    row.grow();
    tk::Column& head = row.add<tk::Column>();
    head.fixWidth(theme.spacing.sidebarWidth);
    head.pad(tk::Insets{theme.spacing.gap, pad, 0.0f, 0.0f}).space(theme.spacing.gap);
    tk::Card& pane = row.add<tk::Card>();
    pane.grow();
    pane.pad(tk::Insets::all(theme.spacing.gap * 2.0f)).space(theme.spacing.gap * 2.0f);
    root->add<tk::HintBar>().fixHeight(theme.spacing.hintBarHeight);
    return Frame{std::move(root), &head, &pane};
}

gpu2d::Rect areaFor(EditorLook look, const gpu2d::Rect& viewport, const tk::Theme& theme) {
    if (look == EditorLook::Page) {
        return viewport;
    }
    const float width = std::min(viewport.width * panelShare, theme.px(panelMaxDp));
    return gpu2d::Rect{viewport.x + viewport.width - width, viewport.y, width, viewport.height};
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

LayeredEditor::LayeredEditor(LayeredSettings& settings, tk::Navigator& navigator,
                             const ui::Chrome& chrome, EditorContext context, EditorLook look,
                             std::string title)
    : Screen{"layered", std::move(title)}, settings_{settings}, navigator_{navigator},
      chrome_{chrome}, context_{std::move(context)}, look_{look},
      schema_{settings_.schemaOf(context_)}, scope_{context_.scopes.size() - 1} {
}

std::string LayeredEditor::scopeName() const {
    return settings_.scopeName(scope());
}

void LayeredEditor::showScope(std::size_t index) {
    if (index < context_.scopes.size()) {
        scope_ = index;
    }
}

std::string LayeredEditor::labelOf(const Setting& setting, const std::string& value) const {
    return setting.type == SettingType::Path ? presetLabel(value) : setting.labelOf(value);
}

std::string LayeredEditor::useText(const settings::ScopedValue& value) const {
    const std::string label = labelOf(*value.setting, value.inherited.value);
    if (!value.inherited.from) {
        return "Use the default: " + label;
    }
    return "Use " + label + " from " + settings_.scopeName(*value.inherited.from);
}

void LayeredEditor::goTo(const Layer& layer) {
    const auto at = std::ranges::find(context_.scopes, layer);
    if (at != context_.scopes.end()) {
        scope_ = static_cast<std::size_t>(at - context_.scopes.begin());
    }
}

std::unique_ptr<tk::Widget> LayeredEditor::build(const tk::Theme& theme) {
    Frame frame = frameFor(look_, theme, chrome_);
    frame.head->add<tk::Label>(title(), tk::TextRole::Title, look_ == EditorLook::Page);
    const std::size_t kept = settings::keptCount(settings_.rows(schema_, context_, scope()));
    frame.head->add<tk::Label>(kept == 0 ? "Nothing set for " + phrased(settings_, scope())
                                         : countOf(kept, "custom setting"),
                               tk::TextRole::Caption);
    if (context_.scopes.size() > 1) {
        std::vector<tk::Tabs::Tab> tabs;
        tabs.reserve(context_.scopes.size());
        for (const Layer& layer : context_.scopes) {
            tabs.push_back(
                tk::Tabs::Tab{layer.id.empty() ? "global" : layer.id, settings_.scopeName(layer)});
        }
        frame.body->add<tk::Tabs>(
            std::string{scopesId}, std::move(tabs), scope_,
            [this](std::size_t index) {
                scope_ = index;
            },
            tk::Orientation::Horizontal);
    }
    tk::List& list = frame.body->add<tk::List>("rows", theme.spacing.gap);
    list.grow();
    addRows(list.rows(), theme);
    return std::move(frame.root);
}

void LayeredEditor::addRows(tk::Column& rows, const tk::Theme& /*theme*/) {
    const std::vector<settings::ScopedValue> values = settings_.rows(schema_, context_, scope());
    firstRow_.clear();
    std::string section;
    for (const settings::ScopedValue& value : values) {
        if (value.setting->section != section) {
            section = value.setting->section;
            rows.add<tk::SectionHeading>(section);
            if (section == settings::frameGenSection) {
                addFrameGenSummary(rows);
            } else if (section == settings::scalingSection) {
                addScalingSummary(rows);
            }
        }
        if (firstRow_.empty()) {
            firstRow_ = rowId(value.setting->key);
        }
        addRow(rows, value);
    }
    if (values.empty()) {
        rows.add<tk::Item>(
            "empty", tk::Item::Text{"Nothing to set here", "None of these settings are kept for " +
                                                               phrased(settings_, scope())});
    }
    addClear(rows);
}

void LayeredEditor::addFrameGenSummary(tk::Column& rows) {
    const settings::FrameGenSettings applies = settings::resolveFrameGen(
        settings::scopeLookup(schema_, settings_.layersOf(), context_.keys, scope()), scopeName());
    rows.add<tk::Item>("framegen.summary", tk::Item::Text{settings::frameGenSummary(applies),
                                                          settings_.frameGenSource()});
}

void LayeredEditor::addScalingSummary(tk::Column& rows) {
    const std::string summary = settings_.scalingSummary(
        settings::scopeLookup(schema_, settings_.layersOf(), context_.keys, scope()), scopeName());
    rows.add<tk::Item>("scaling.summary", tk::Item::Text{summary, settings_.scalingSource()});
}

void LayeredEditor::addRow(tk::Column& rows, const settings::ScopedValue& value) {
    const Setting& setting = *value.setting;
    const std::string id = rowId(setting.key);
    const Layer here = scope();
    std::string subtitle = setting.help;
    if (value.overriddenBy) {
        subtitle = settings_.scopeName(*value.overriddenBy) + " uses " +
                   labelOf(setting, value.overridingValue) + " over this";
    }
    const tk::Item::Text text{setting.label, std::move(subtitle)};
    tk::Item* row = nullptr;
    switch (setting.type) {
    case SettingType::Choice:
    case SettingType::Toggle:
    case SettingType::Preset: {
        // Place 0 uses what the scope inherits; the inherited value is not offered again unless
        // the scope keeps it.
        std::vector<std::string> options{labelOf(setting, value.inherited.value)};
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
        auto& choice = rows.add<tk::Choice>(id, text, std::move(options), chosen,
                                            [this, here, key = setting.key, keys = std::move(keys),
                                             own = value.own, chosen](std::size_t picked) {
                                                if (picked == chosen) {
                                                    return;
                                                }
                                                if (picked == 0) {
                                                    if (own) {
                                                        settings_.remove(here, key);
                                                    }
                                                    return;
                                                }
                                                settings_.set(context_, here, key, keys[picked]);
                                            });
        choice.listAs(std::move(listed));
        row = &choice;
        break;
    }
    case SettingType::Slider: {
        auto& slider = rows.add<tk::Slider>(
            id, text, tk::Slider::Range{setting.range.low, setting.range.high, setting.range.step},
            levelOf(value.value, setting.range), [this, here, key = setting.key](int level) {
                settings_.set(context_, here, key, std::to_string(level));
            });
        slider.unit(setting.unit);
        row = &slider;
        break;
    }
    case SettingType::Path: {
        auto& item = rows.add<tk::Item>(id, text);
        item.value(labelOf(setting, value.value))
            .onPress(
                [this, here] {
                    navigator_.push(std::make_unique<ShaderPresetScreen>(
                        settings_, navigator_, chrome_, context_, here, look_));
                },
                "Choose", true);
        row = &item;
        break;
    }
    }
    row->chip(value.from ? settings_.scopeName(*value.from) : std::string{"Default"}, value.own);
    if (value.from && *value.from != here &&
        std::ranges::find(context_.scopes, *value.from) != context_.scopes.end()) {
        row->onAction(
            tk::Action::Secondary,
            [this, from = *value.from] {
                goTo(from);
            },
            "Go to " + settings_.scopeName(*value.from));
    }
    if (value.own) {
        row->onAction(
            tk::Action::Tertiary,
            [this, here, key = setting.key] {
                settings_.remove(here, key);
            },
            useText(value));
    } else if (const std::string* removed = settings_.removed(here, setting.key)) {
        row->onAction(
            tk::Action::Tertiary,
            [this, here, key = setting.key] {
                settings_.restore(here, key);
            },
            "Restore " + labelOf(setting, *removed));
    }
}

void LayeredEditor::addClear(tk::Column& rows) {
    const Layer here = scope();
    const std::size_t kept = settings::keptCount(settings_.rows(schema_, context_, here));
    if (kept == 0) {
        return;
    }
    const bool game = here.kind == Layer::Kind::Game;
    const std::string whose = game ? std::string{"this game's"} : scopeName();
    const std::string under = here.kind == Layer::Kind::Global ? std::string{"its default"}
                                                               : std::string{"what it inherits"};
    rows.add<tk::SectionHeading>("Reset");
    rows.add<tk::Item>(std::string{clearId},
                       tk::Item::Text{"Clear " + whose + " settings",
                                      "Every value set here goes back to " + under})
        .value(countOf(kept, "setting"))
        .onPress([this, here, whose, under] {
            navigator_.push(tk::confirm("Clear " + whose + " settings?",
                                        "Every value set here goes back to " + under + ".", "Clear",
                                        [this, here] {
                                            settings_.clear(here);
                                        }));
        });
}

gpu2d::Rect LayeredEditor::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    return areaFor(look_, viewport, theme);
}

std::vector<tk::Hint> LayeredEditor::hints() const {
    std::vector<tk::Hint> hints{tk::Hint{{tk::Action::Back}, "Back"}};
    if (context_.scopes.size() > 1) {
        hints.push_back(tk::Hint{{tk::Action::PreviousTab, tk::Action::NextTab}, "Scope"});
    }
    return hints;
}

std::string LayeredEditor::initialFocus() const {
    return firstRow_;
}

ShaderPresetScreen::ShaderPresetScreen(LayeredSettings& settings, tk::Navigator& navigator,
                                       const ui::Chrome& chrome, EditorContext context, Layer scope,
                                       EditorLook look)
    : Screen{"shader-presets", {}}, settings_{settings}, navigator_{navigator}, chrome_{chrome},
      context_{std::move(context)}, scope_{std::move(scope)}, look_{look},
      presets_{settings_.shaders().presets()} {
}

void ShaderPresetScreen::open(const std::string& folder) {
    folder_ = folder;
    const PresetFolder shown = presetFolder(presets_, folder_);
    if (!shown.folders.empty()) {
        navigator_.focus("folder/" + shown.folders.front().path);
    } else if (!shown.presets.empty()) {
        navigator_.focus("preset/" + shown.presets.front().path);
    }
}

std::string ShaderPresetScreen::title() const {
    return folder_.empty() ? std::string{"Shader preset"}
                           : "Shader preset › " + presetCrumbs(folder_);
}

void ShaderPresetScreen::pick(const std::string* value) {
    if (value == nullptr) {
        settings_.remove(scope_, std::string{settings::shaderKey});
    } else {
        settings_.set(context_, scope_, std::string{settings::shaderKey}, *value);
    }
    close();
}

std::unique_ptr<tk::Widget> ShaderPresetScreen::build(const tk::Theme& theme) {
    Frame frame = frameFor(look_, theme, chrome_);
    frame.head->add<tk::Label>(title(), tk::TextRole::Title, look_ == EditorLook::Page);
    frame.head->add<tk::Label>("For " + phrased(settings_, scope_), tk::TextRole::Caption);
    tk::List& list = frame.body->add<tk::List>("presets/" + folder_, theme.spacing.gap);
    list.grow();
    tk::Column& rows = list.rows();
    const settings::SettingLayers& layers = settings_.layersOf();
    const std::string* own = layers.at(scope_, settings::shaderKey);
    if (folder_.empty()) {
        const settings::Resolved inherited =
            scope_.kind == Layer::Kind::Global
                ? settings::Resolved{}
                : layers.inherited(context_.keys, scope_, settings::shaderKey, std::string{});
        rows.add<tk::Item>(
                "inherit",
                tk::Item::Text{inherited.from ? "Use " + presetLabel(inherited.value) + " from " +
                                                    settings_.scopeName(*inherited.from)
                                              : "Use the default: None",
                               "Removes the preset set for " + phrased(settings_, scope_)})
            .mark(own == nullptr)
            .onPress([this] {
                pick(nullptr);
            });
        rows.add<tk::Item>("none", tk::Item::Text{"None", "No preset; Sampling and Filter apply"})
            .mark(own != nullptr && own->empty())
            .onPress([this] {
                const std::string none;
                pick(&none);
            });
        if (presets_.empty()) {
            rows.add<tk::Item>("missing",
                               tk::Item::Text{"No presets downloaded",
                                              "Download Shader presets in Settings › Emulators"});
        }
    }
    const PresetFolder shown = presetFolder(presets_, folder_);
    for (const PresetFolder::Child& child : shown.folders) {
        const bool chosen = own != nullptr && presetBelow(*own, child.path);
        rows.add<tk::Item>("folder/" + child.path, tk::Item::Text{child.name, {}})
            .value(chosen ? "Chosen: " + library::roms::presetName(*own)
                          : countOf(child.presets, "preset"))
            .onPress(
                [this, path = child.path] {
                    open(path);
                },
                "Open", true);
    }
    for (const library::roms::ShaderPreset& preset : shown.presets) {
        rows.add<tk::Item>("preset/" + preset.path, tk::Item::Text{preset.name, preset.path})
            .mark(own != nullptr && *own == preset.path)
            .onPress([this, path = preset.path] {
                pick(&path);
            });
    }
    return std::move(frame.root);
}

gpu2d::Rect ShaderPresetScreen::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    return areaFor(look_, viewport, theme);
}

tk::Screen::BackReply ShaderPresetScreen::back(tk::ActContext& context) {
    if (folder_.empty()) {
        return BackReply::Pop;
    }
    const std::string left = "folder/" + folder_;
    const std::size_t slash = folder_.rfind('/');
    folder_.resize(slash == std::string::npos ? 0 : slash);
    context.focus(left);
    return BackReply::Stay;
}

std::string ShaderPresetScreen::initialFocus() const {
    const std::string* own = settings_.layersOf().at(scope_, settings::shaderKey);
    if (own != nullptr && own->empty()) {
        return "none";
    }
    // The folder holding the preset kept, or the first one when the scope keeps none.
    const PresetFolder top = presetFolder(presets_, {});
    for (const PresetFolder::Child& child : top.folders) {
        if (own == nullptr || presetBelow(*own, child.path)) {
            return "folder/" + child.path;
        }
    }
    return own == nullptr ? std::string{"inherit"} : "preset/" + *own;
}

} // namespace opensu::app
