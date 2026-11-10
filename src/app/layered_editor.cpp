#include "layered_editor.hpp"

#include <algorithm>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "lucent/text.h"
#include "setting_sheet.hpp"
#include "settings/framegen_settings.hpp"
#include "settings/scaling_settings.hpp"
#include "shader_preset_tree.hpp"
#include "ui/page_frame.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;
using settings::Layer;
using settings::Setting;
using settings::SettingType;

constexpr std::string_view rowPrefix = "row/";
constexpr std::string_view clearId = "clear";
/// The panel's share of the window's width, and the most it takes.
constexpr float panelShare = 0.5f;
constexpr float panelMaxDp = 720.0f;

/// `scope` as a sentence names it: "this game", "Game Boy Advance".
std::string phrased(const LayeredSettings& settings, const Layer& scope) {
    return scope.kind == Layer::Kind::Game ? std::string{"this game"} : settings.scopeName(scope);
}

/// The row opening group `name`'s page.
std::string groupId(std::string_view name) {
    return "group/" + std::string{name};
}

std::string rowId(std::string_view key) {
    return std::string{rowPrefix} + std::string{key};
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
        card->add<tk::HintBar>();
        tk::Column* bodyAt = &body;
        tk::Column* headAt = &head;
        return Frame{std::move(card), headAt, bodyAt};
    }
    auto root = std::make_unique<tk::Backdrop>();
    root->space(theme.spacing.gap);
    root->add<tk::Spacer>(0.0f).fixHeight(chrome.top / theme.scale);
    if (ui::narrow(theme, chrome)) {
        // The head above the pane: a narrow screen has no room for a side column.
        tk::Column& column = root->add<tk::Column>();
        column.pad(tk::Insets{pad, 0.0f, pad, 0.0f}).space(theme.spacing.gap);
        column.grow();
        tk::Column& head = column.add<tk::Column>();
        head.pad(tk::Insets{pad, 0.0f, 0.0f, 0.0f}).space(theme.spacing.gap);
        tk::Card& pane = column.add<tk::Card>();
        pane.grow();
        pane.pad(tk::Insets::all(theme.spacing.gap * 2.0f)).space(theme.spacing.gap * 2.0f);
        root->add<tk::HintBar>();
        return Frame{std::move(root), &head, &pane};
    }
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
    root->add<tk::HintBar>();
    return Frame{std::move(root), &head, &pane};
}

gpu2d::Rect areaFor(EditorLook look, const gpu2d::Rect& viewport, const tk::Theme& theme,
                    const ui::Chrome& chrome) {
    // A panel on a narrow screen takes it all; half of it holds no row.
    if (look == EditorLook::Page || ui::narrow(theme, chrome)) {
        return viewport;
    }
    const float width = std::min(viewport.width * panelShare, theme.px(panelMaxDp));
    return gpu2d::Rect{viewport.x + viewport.width - width, viewport.y, width, viewport.height};
}

} // namespace

LayeredEditor::LayeredEditor(LayeredSettings& settings, tk::Navigator& navigator,
                             const ui::Chrome& chrome, EditorContext context, EditorLook look,
                             std::string title, EditorRows more, std::string group)
    : Screen{group.empty() ? std::string{"layered"} : "layered/" + group, std::move(title)},
      settings_{settings}, navigator_{navigator}, chrome_{chrome}, context_{std::move(context)},
      look_{look}, schema_{settings_.schemaOf(context_)}, more_{std::move(more)},
      group_{std::move(group)} {
}

std::unique_ptr<tk::Widget> LayeredEditor::build(const tk::Theme& theme) {
    Frame frame = frameFor(look_, theme, chrome_);
    frame.head->add<tk::Label>(title(), tk::TextRole::Title, look_ == EditorLook::Page);
    const std::vector<settings::ScopedValue> values = settings_.rows(schema_, context_, scope());
    const auto changed = static_cast<std::size_t>(
        std::ranges::count_if(values, [](const settings::ScopedValue& value) {
            return value.from.has_value();
        }));
    frame.head->add<tk::Label>(changed == 0 ? std::string{"Every setting at its default"}
                                            : lucent::text::counted(changed, "changed setting"),
                               tk::TextRole::Caption);
    tk::List& list = frame.body->add<tk::List>("rows", theme.spacing.gap);
    list.grow();
    addRows(list.rows(), theme);
    return std::move(frame.root);
}

void LayeredEditor::addRows(tk::Column& rows, const tk::Theme& /*theme*/) {
    const std::vector<settings::ScopedValue> values = settings_.rows(schema_, context_, scope());
    firstRow_.clear();
    std::string section;
    std::vector<std::string> listed;
    for (const settings::ScopedValue& value : values) {
        const std::string& group = value.setting->group;
        const bool grouped = group_.empty() && !group.empty();
        if ((!group_.empty() && group != group_) ||
            (grouped && std::ranges::find(listed, group) != listed.end())) {
            continue;
        }
        if (value.setting->section != section) {
            section = value.setting->section;
            rows.add<tk::SectionHeading>(section);
            addSummary(rows, section);
        }
        if (grouped) {
            listed.push_back(group);
            if (firstRow_.empty()) {
                firstRow_ = groupId(group);
            }
            addGroup(rows, values, group);
            continue;
        }
        if (firstRow_.empty()) {
            firstRow_ = rowId(value.setting->key);
        }
        addRow(rows, value);
    }
    if (!group_.empty()) {
        return;
    }
    const std::vector<ui::SettingsRow> more =
        more_.rows ? more_.rows() : std::vector<ui::SettingsRow>{};
    if (values.empty() && more.empty()) {
        rows.add<tk::Item>(
            "empty", tk::Item::Text{"Nothing to set here", "None of these settings are kept for " +
                                                               phrased(settings_, scope())});
    }
    if (!more.empty()) {
        rows.add<tk::SectionHeading>("This game");
        if (firstRow_.empty()) {
            firstRow_ = more.front().id;
        }
    }
    for (const ui::SettingsRow& row : more) {
        ui::addRow(rows, row, *more_.actions);
    }
    addClear(rows);
}

void LayeredEditor::addGroup(tk::Column& rows, const std::vector<settings::ScopedValue>& values,
                             const std::string& group) {
    std::size_t settings = 0;
    std::size_t kept = 0;
    for (const settings::ScopedValue& value : values) {
        if (value.setting->group == group) {
            ++settings;
            kept += value.from ? 1U : 0U;
        }
    }
    rows.add<tk::Item>(groupId(group),
                       tk::Item::Text{group, kept == 0
                                                 ? lucent::text::counted(settings, "setting")
                                                 : lucent::text::counted(kept, "changed setting")})
        .onPress(
            // The name lives in `schema_`, so the closure copies nothing that can throw.
            [this, name = &group] {
                navigator_.push(std::make_unique<LayeredEditor>(
                    settings_, navigator_, chrome_, context_, look_, *name, EditorRows{}, *name));
            },
            "Open", true);
}

void LayeredEditor::addSummary(tk::Column& rows, const std::string& section) {
    std::optional<PresentationSummary> summary = settings_.summaryOf(
        section, settings::scopeLookup(schema_, settings_.layersOf(), context_.keys, scope()),
        settings_.scopeName(scope()));
    if (summary) {
        rows.add<tk::Item>(summary->id,
                           tk::Item::Text{std::move(summary->line), std::move(summary->note)});
    }
}

void LayeredEditor::addRow(tk::Column& rows, const settings::ScopedValue& value) {
    const Setting& setting = *value.setting;
    rows.add<tk::Item>(rowId(setting.key), tk::Item::Text{setting.label, setting.help})
        .value(valueLabel(setting, value.value))
        .chip(value.from ? settings_.scopeName(*value.from) : std::string{"Default"},
              value.from.has_value())
        .onPress(
            [this, key = setting.key, label = setting.label] {
                navigator_.push(std::make_unique<SettingSheet>(settings_, navigator_, chrome_,
                                                               context_, look_, key, label));
            },
            "Change", true);
}

void LayeredEditor::addClear(tk::Column& rows) {
    const Layer here = scope();
    const std::size_t kept = settings::keptCount(settings_.rows(schema_, context_, here));
    if (kept == 0) {
        return;
    }
    const bool game = here.kind == Layer::Kind::Game;
    const std::string whose = game ? std::string{"this game's"} : settings_.scopeName(here);
    const std::string under = here.kind == Layer::Kind::Global ? std::string{"its default"}
                                                               : std::string{"what it inherits"};
    rows.add<tk::SectionHeading>("Reset");
    rows.add<tk::Item>(std::string{clearId},
                       tk::Item::Text{"Clear " + whose + " settings",
                                      "Every value set here goes back to " + under})
        .value(lucent::text::counted(kept, "setting"))
        .onPress([this, here = here, whose = whose, under = under] {
            navigator_.push(tk::confirm("Clear " + whose + " settings?",
                                        "Every value set here goes back to " + under + ".", "Clear",
                                        [this, here = here] {
                                            settings_.clear(here);
                                        }));
        });
}

gpu2d::Rect LayeredEditor::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    return areaFor(look_, viewport, theme, chrome_);
}

std::vector<tk::Hint> LayeredEditor::hints() const {
    std::vector<tk::Hint> hints{tk::Hint{{tk::Action::Back}, "Back"}};
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
                          : lucent::text::counted(child.presets, "preset"))
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
    return areaFor(look_, viewport, theme, chrome_);
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
