// dock_motion — the dock's two motions as functions of time: sliding in and out, and the selected
// icon's pop (navigation.md §1.4, §1.6, §1.7). Times are milliseconds on one monotonic clock.
#pragma once

namespace opensu::ui {

/// Whether the dock is on screen, and how far it has slid in. On Home it is always shown; on
/// another section only for `revealMs` after L1 or R1 (iiSU `jk2.java:937`).
class DockVisibility {
  public:
    /// How long an L1 or R1 press holds the dock up.
    static constexpr double revealMs = 1200.0;
    /// The slide in and the slide out, as the dock moves in iiSU's recordings (navigation.md
    /// §5.1): an ease-out that settles in 170 ms, an ease-in that is gone in 125 ms. The code's
    /// `tween(250 ms)` of `iq3` is the envelope; the visible motion is shorter.
    static constexpr double showMs = 170.0;
    static constexpr double hideMs = 125.0;

    /// Whether the section keeps the dock up on its own.
    void setPinned(bool pinned, double nowMs);

    /// Shows the dock until `revealMs` after `nowMs`.
    void reveal(double nowMs);

    /// How far in the dock is at `nowMs`, 0 (out) to 1 (in). The slide and the fade share it.
    [[nodiscard]] float progress(double nowMs) const;

  private:
    /// A slide: where it started from, when, and where it heads.
    struct Slide {
        float from{1.0f};
        double startedMs{-showMs};
        bool heading{true};
    };

    /// The slide as it stands at `nowMs`: a reveal that has run out starts the slide out at the
    /// moment it ran out, not when it was noticed.
    [[nodiscard]] Slide settled(double nowMs) const;
    [[nodiscard]] static float at(const Slide& slide, double nowMs);
    /// Turns the dock towards `heading` at `nowMs` from wherever it is.
    void turn(bool heading, double nowMs);

    bool pinned_{true};
    double revealUntilMs_{-1.0};
    Slide slide_;
};

/// A dock icon's scale as its section is selected and deselected (iiSU `p90.java:186-216`).
class IconPop {
  public:
    static constexpr float restSelected = 1.04f;
    static constexpr float peak = 1.06f;
    static constexpr double riseMs = 120.0;
    static constexpr double settleMs = 110.0;
    static constexpr double releaseMs = 90.0;

    /// Starts selected, as an item that is selected when it first composes is (`nl2.java:2437`).
    explicit IconPop(bool selected = false) noexcept;

    /// Selects or deselects the icon at `nowMs`; a repeat of the current state does nothing.
    void select(bool selected, double nowMs) noexcept;

    [[nodiscard]] float scale(double nowMs) const noexcept;
    [[nodiscard]] bool selected() const noexcept {
        return selected_;
    }

  private:
    bool selected_;
    /// Long ago, so a pop is not played for the state an item starts in.
    double changedAtMs_{-1.0e9};
    float from_;
};

} // namespace opensu::ui
