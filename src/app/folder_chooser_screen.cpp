#include "folder_chooser_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/page_frame.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's width, in dp.
constexpr float cardDp = 520.0f;
/// The most bytes a typed path holds: Linux's PATH_MAX.
constexpr std::size_t pathMostLength = 4096;

std::string fileIdOf(const ui::FolderEntry& entry) {
    return "folders/file/" + entry.name;
}

} // namespace

std::string FolderChooserScreen::landing(const PathEditor& editor) {
    const ui::FolderChooser& folders = editor.folders();
    if (!editor.request().files) {
        return useId;
    }
    if (!folders.files().empty()) {
        return fileIdOf(folders.files().front());
    }
    return folders.hasParent() ? std::string{"folders/up"} : std::string{fieldId};
}

std::unique_ptr<tk::Widget> FolderChooserScreen::build(const tk::Theme& theme) {
    ui::CardFrame frame = ui::tallCard(theme, chrome_, cardDp);
    tk::Card& card = *frame.card;
    const ui::FolderChooser& folders = editor_.folders();
    card.add<tk::Label>(title(), tk::TextRole::Title);
    card.add<tk::TextField>(fieldId, "Path", editor_.typed(),
                            [this](std::string text) {
                                editor_.type(std::move(text));
                            })
        .placeholder(editor_.request().files ? "Type a file path" : "Type a folder path")
        .limit(pathMostLength)
        .onSubmit(
            [this] {
                editor_.submitTyped();
            },
            "Use path");
    if (!editor_.refusal().empty()) {
        card.add<tk::Label>(editor_.refusal(), tk::TextRole::Caption, true);
    } else if (!folders.problem().empty()) {
        card.add<tk::Label>(folders.problem(), tk::TextRole::Caption, true);
    }
    tk::List& list = card.add<tk::List>("folders/list", theme.spacing.gap);
    list.grow();
    tk::Column& rows = list.rows();
    if (!editor_.request().files) {
        rows.add<tk::Item>(useId, tk::Item::Text{"Use this folder", ""}).onPress([this] {
            editor_.useShown();
        });
    }
    if (const std::string& drop = editor_.request().clearLabel; !drop.empty()) {
        rows.add<tk::Item>("folders/drop", tk::Item::Text{drop, ""}).onPress([this] {
            editor_.dropChoice();
        });
    }
    if (folders.hasParent()) {
        rows.add<tk::Item>("folders/up", tk::Item::Text{"Up one level", ""})
            .onPress(
                [this] {
                    editor_.up();
                },
                "Open");
    }
    for (const ui::FolderEntry& entry : folders.folders()) {
        rows.add<tk::Item>("folders/dir/" + entry.name, tk::Item::Text{entry.name, ""})
            .onPress(
                [this, path = entry.path] {
                    editor_.enter(path);
                },
                "Open", true);
    }
    for (const ui::FolderEntry& entry : folders.files()) {
        rows.add<tk::Item>(fileIdOf(entry), tk::Item::Text{entry.name, ""})
            .onPress(
                [this, path = entry.path] {
                    editor_.pick(path);
                },
                "Choose");
    }
    card.add<tk::HintBar>().fixHeight(theme.spacing.hintBarHeight);
    return std::move(frame.root);
}

void FolderChooserScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply FolderChooserScreen::back(tk::ActContext& /*context*/) {
    editor_.cancel();
    return BackReply::Stay;
}

bool FolderChooserScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    return action == tk::Action::Left && editor_.up();
}

std::vector<tk::Hint> FolderChooserScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Cancel"}, tk::Hint{{tk::Action::Left}, "Up a level"}};
}

} // namespace opensu::app
