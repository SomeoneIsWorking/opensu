// home_dock — iiSU's primary navigation bar (`gh3.K`, navigation.md §1): the chosen section, the
// bar's slide in and out and its icons' pops, drawn as glass over the home screen. It is a chrome
// screen of the navigator, drawn after the scene its glass blurs; while Home is the top screen its
// tabs (a `gpu2d::ui::TabDock`) take the pointer, and resting on the bar slides it in.
#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include "gpu2d/render_target.hpp"
#include "gpu2d/ui/screen.hpp"

#include "dock_metrics.hpp"
#include "dock_motion.hpp"
#include "dock_painter.hpp"
#include "library/sections.hpp"

namespace opensu::ui {

class HomeDock final : public gpu2d::ui::Screen {
  public:
    using Clock = std::chrono::steady_clock;

    /// What the dock stands in this frame.
    struct Frame {
        float width{};
        float height{};
        float dp{1.0f};
        Clock::time_point now{};
        /// Whether it is drawn: Home in front, no page and no keyboard over it.
        bool shown{false};
        /// Whether its tabs take the pointer and LB and RB: Home is the top screen.
        bool live{false};
    };

    HomeDock(const input::Prompts& prompts, gpu2d::ui::Typesetter& text, IconAtlas& icons);

    /// What a press on a tab chooses; set once by the app.
    void onChoose(std::function<void(library::Section)> chose) {
        chose_ = std::move(chose);
    }
    /// Before the navigator lays it out.
    void setFrame(const Frame& frame) noexcept {
        frame_ = frame;
    }
    /// The scene its glass blurs, drawn this frame; null draws no glass.
    void setScene(const gpu2d::RenderTarget* scene) noexcept {
        scene_ = scene;
    }
    /// Makes `section` the chosen one; `reveal` slides the bar in for a moment (iiSU
    /// `jk2.java:937`), as an L1 or R1 press does.
    void setSection(library::Section section, bool reveal);
    /// Whether the bar stays up on Library (the picker's "Pin navigation bar" option).
    void setPinLibrary(bool pinned);
    /// The highest point this frame draws the dock at, or nothing while it is not drawn.
    [[nodiscard]] std::optional<float> top() const;

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;

  private:
    struct Geometry {
        DockMetrics metrics;
        DockLayout layout;
        DockStyle style;
    };
    [[nodiscard]] double nowMs() const noexcept;
    [[nodiscard]] Geometry geometry() const;
    /// Slides a hidden bar in while the pointer rests on it.
    void rest();
    void paint(gpu2d::ui::Painter& painter, std::optional<std::size_t> hovered);

    DockPainter painter_;
    Frame frame_;
    const gpu2d::RenderTarget* scene_{};
    std::function<void(library::Section)> chose_;
    library::Section section_{library::Section::Home};
    bool pinLibrary_{true};
    DockVisibility visibility_;
    std::array<IconPop, library::allSections.size()> pops_{IconPop{true}, IconPop{false}};
};

} // namespace opensu::ui
