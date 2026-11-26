// icon_widgets — the toolkit's row and button with one of the shipped icons (`Icon`) drawn in the
// words' colour: a row's at its end, after its value; a button's at its left, its label centred as
// any button's; a tab's alone, the chosen tab filled. The stream icon on every Play that streams
// the game and Quick settings' tabs are drawn through them.
#pragma once

#include <functional>
#include <string>
#include <utility>

#include "gpu2d/ui/widgets.hpp"

#include "screen_images.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

class IconItem final : public gpu2d::ui::Item {
  public:
    IconItem(std::string id, Text text, ScreenImages& images, Icon icon)
        : Item{std::move(id), std::move(text)}, images_{images}, icon_{icon} {
    }

  protected:
    [[nodiscard]] float trailingWidth(gpu2d::ui::LayoutContext& context, float width) override;
    void paintTrailing(gpu2d::ui::Painter& painter, const gpu2d::Rect& area) const override;

  private:
    ScreenImages& images_;
    Icon icon_;
};

class IconButton final : public gpu2d::ui::Button {
  public:
    IconButton(std::string id, std::string label, std::function<void()> pressed,
               gpu2d::ui::ButtonKind kind, ScreenImages& images, Icon icon)
        : Button{std::move(id), std::move(label), std::move(pressed), kind}, kind_{kind},
          images_{images}, icon_{icon} {
    }

    [[nodiscard]] gpu2d::ui::Size measure(gpu2d::ui::LayoutContext& context,
                                          float maxWidth) override;
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    gpu2d::ui::ButtonKind kind_;
    ScreenImages& images_;
    Icon icon_;
};

/// A tab shown by its icon alone, square; the chosen one is filled. `verb` is A's hint, never
/// drawn.
class IconTab final : public gpu2d::ui::Button {
  public:
    IconTab(std::string id, std::string verb, std::function<void()> pressed, bool chosen,
            ScreenImages& images, Icon icon)
        : Button{std::move(id), std::move(verb), std::move(pressed),
                 chosen ? gpu2d::ui::ButtonKind::Primary : gpu2d::ui::ButtonKind::Normal},
          images_{images}, icon_{icon} {
    }

    [[nodiscard]] gpu2d::ui::Size measure(gpu2d::ui::LayoutContext& context,
                                          float maxWidth) override;
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    ScreenImages& images_;
    Icon icon_;
};

} // namespace opensu::ui
