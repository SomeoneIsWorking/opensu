#include "home_dock.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/tab_dock.hpp"

#include "chrome_panel.hpp"
#include "hud.hpp"

namespace opensu::ui {
namespace {

// iiSU's backdrop blur radius for glass (`homeGlassBlurRadiusDp`), as a Gaussian's sigma.
constexpr float glassBlurSigmaDp = 8.0f;

constexpr std::string_view tabsId = "dock/sections";

/// The sections' tabs where the bar draws its items, the bar's full height and slid with it;
/// resting on the bar where it rests asks for it, and a press there while it is out of sight
/// brings it in rather than choosing (iiSU g23: "Tap the bottom middle to bring back the navbar").
class SectionTabs final : public gpu2d::ui::TabDock {
  public:
    SectionTabs(std::size_t chosen, std::function<void(std::size_t)> chose, DockLayout layout,
                float slide, bool hidden, std::function<void(bool)> rest)
        : TabDock{std::string{tabsId}, Tabs{.count = library::allSections.size(), .chosen = chosen},
                  std::move(chose)},
          layout_{std::move(layout)}, slide_{slide}, hidden_{hidden}, rest_{std::move(rest)} {
    }
    bool press(gpu2d::Point at, gpu2d::ui::ActContext& context) override {
        if (hidden_) {
            rest_(true);
            return true;
        }
        return TabDock::press(at, context);
    }

    void arrange(gpu2d::ui::LayoutContext& context, const gpu2d::Rect& /*frame*/) override {
        frame_ = onCanvas(layout_.bar);
        for (std::size_t i = 0; i < count() && i < layout_.items.size(); ++i) {
            const Rect& item = layout_.items[i];
            place(context, i,
                  gpu2d::Rect{item.x, layout_.bar.y + slide_, item.width, layout_.bar.height});
        }
    }
    void hover(gpu2d::Point at, gpu2d::ui::ActContext& /*context*/) override {
        if (layout_.bar.contains(at.x, at.y)) {
            rest_(false);
        }
    }

  private:
    DockLayout layout_;
    float slide_;
    bool hidden_;
    std::function<void(bool)> rest_;
};

} // namespace

HomeDock::HomeDock(const input::Prompts& prompts, gpu2d::ui::Typesetter& text, IconAtlas& icons)
    : Screen{"dock", "Sections"}, painter_{prompts, text, icons} {
}

double HomeDock::nowMs() const noexcept {
    return std::chrono::duration<double, std::milli>(frame_.now.time_since_epoch()).count();
}

void HomeDock::setSection(library::Section section, bool reveal) {
    section_ = section;
    const double now = nowMs();
    visibility_.setPinned(dockPinned(section, pinLibrary_), now);
    if (reveal) {
        visibility_.reveal(now);
    }
    for (std::size_t i = 0; i < library::allSections.size(); ++i) {
        pops_[i].select(library::allSections[i] == section, now);
    }
}

void HomeDock::setPinLibrary(bool pinned) {
    pinLibrary_ = pinned;
    visibility_.setPinned(dockPinned(section_, pinLibrary_), nowMs());
}

HomeDock::Geometry HomeDock::geometry() const {
    const float dp = frame_.dp;
    const std::vector<float> widths{sectionItems.begin(), sectionItems.end()};
    const DockMetrics metrics{frame_.width / dp, frame_.height / dp, widths};
    return Geometry{metrics, layoutDock(metrics, frame_.width, frame_.height, dp),
                    DockStyle{frame_.dark, dp, visibility_.progress(nowMs())}};
}

std::optional<float> HomeDock::top() const {
    if (!frame_.shown) {
        return std::nullopt;
    }
    const Geometry dock = geometry();
    return DockPainter::top(dock.layout, dock.style);
}

void HomeDock::rest(bool tapped) {
    visibility_.reveal(nowMs(), tapped ? DockVisibility::touchRevealMs : DockVisibility::revealMs);
}

std::unique_ptr<gpu2d::ui::Widget> HomeDock::build(const gpu2d::ui::Theme& /*theme*/) {
    const Geometry dock = geometry();
    auto root = std::make_unique<ChromePanel>([this](gpu2d::ui::Painter& painter) {
        paint(painter, gpu2d::ui::TabDock::tabOf(tabsId, painter.hovered));
    });
    if (frame_.live) {
        const float slide = DockPainter::barRect(dock.layout, dock.style).y - dock.layout.bar.y;
        const auto chosen = static_cast<std::size_t>(
            std::ranges::find(library::allSections, section_) - library::allSections.begin());
        root->part(std::make_unique<SectionTabs>(
                       chosen,
                       [this](std::size_t index) {
                           if (chose_) {
                               chose_(library::allSections[index]);
                           }
                       },
                       dock.layout, slide, visibility_.hidden(nowMs()),
                       [this](bool tapped) {
                           rest(tapped);
                       }),
                   onCanvas(dock.layout.bar));
    }
    return root;
}

void HomeDock::paint(gpu2d::ui::Painter& painter, std::optional<std::size_t> hovered) {
    if (!frame_.shown) {
        return;
    }
    const Geometry dock = geometry();
    const double now = nowMs();
    const Rect bar = DockPainter::barRect(dock.layout, dock.style);
    painter.canvas.blurBehind(
        gpu2d::Backdrop{.scene = scene_,
                        .body = onCanvas(bar),
                        .radius = bar.height * 0.5f,
                        .sigma = glassBlurSigmaDp * frame_.dp,
                        .opacity = std::clamp(dock.style.progress, 0.0f, 1.0f)});
    std::array<DockIcon, library::allSections.size()> icons;
    for (std::size_t i = 0; i < icons.size(); ++i) {
        const library::Section section = library::allSections[i];
        icons[i] = DockIcon{.section = section,
                            .selected = section == section_,
                            .scale = pops_[i].scale(now),
                            .hovered = hovered == i};
    }
    painter_.paint(painter.canvas, dock.layout, dock.metrics, icons, dock.style);
}

} // namespace opensu::ui
