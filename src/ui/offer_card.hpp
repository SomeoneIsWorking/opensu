// offer_card — one giveaway on an interface-toolkit screen: its art across the top, its title, a
// line saying the store and when it ends, and a badge ("Owned", "DLC") at the line's end. A opens
// what `pressed` opens.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "gpu2d/ui/widget.hpp"

#include "screen_images.hpp"

namespace opensu::ui {

struct OfferCardText {
    std::string title;
    /// "Epic · Ends in 2 days".
    std::string line;
    /// Empty for none.
    std::string badge;
    /// The badge in the accent colour ("Owned").
    bool strongBadge{false};
};

class OfferCard final : public gpu2d::ui::Widget {
  public:
    /// The art's width over its height.
    static constexpr float artAspect = 16.0f / 9.0f;

    OfferCard(std::string id, ScreenImages& images, std::filesystem::path art, OfferCardText text,
              std::function<void()> pressed)
        : Widget{std::move(id)}, images_{images}, art_{std::move(art)}, text_{std::move(text)},
          pressed_{std::move(pressed)} {
    }

    [[nodiscard]] gpu2d::ui::Size measure(gpu2d::ui::LayoutContext& context,
                                          float maxWidth) override;
    void arrange(gpu2d::ui::LayoutContext& context, const gpu2d::Rect& frame) override;
    [[nodiscard]] bool focusable() const override {
        return true;
    }
    [[nodiscard]] float cornerRadius(const gpu2d::ui::Theme& theme) const override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    ScreenImages& images_;
    std::filesystem::path art_;
    OfferCardText text_;
    std::function<void()> pressed_;
    /// As fitted to the card's width.
    std::string titleShown_;
    std::string lineShown_;
};

} // namespace opensu::ui
