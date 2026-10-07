// top_bar_metrics — sizes of iiSU's single-screen top bar, in dp.
//
// The is7/hs7 sizing objects and the status pill's own scales (home-grid.md §2).
// Pure arithmetic from the screen width in dp: nothing here draws.
#pragma once

namespace iideck::ui {

/// iiSU is7 SingleScreenStatusPillSizing.
struct StatusPillSizing {
    float scale{};
    float stretch{};
    float endPadding{};
};

/// iiSU hs7 SingleScreenHudStatusAlignment.
struct HudStatusAlignment {
    float profileTopPadding{};
    float compactWidthScale{};
    float statusTopPadding{};
    float statusPillHeight{};
};

/// The status pill's sizes (iiSU a32.o), in dp.
struct StatusPillMetrics {
    float height{};
    float width{};
    /// The content scale c that the bell, text and battery icon follow.
    float contentScale{};
    float bellColumn{};
    float ringSize{};
    float textSpacing{};
    float fontSize{};
    float batteryIcon{};
    float glyphSize{};
    /// The R2 glyph's offset from the pill's top-left corner.
    float glyphOffsetX{};
    float glyphOffsetY{};
};

/// The corner hint row's sizes (iiSU mw5.h, jj2.b), in dp.
struct HintRowMetrics {
    float paddingStart{};
    float paddingBottom{};
    float paddingHorizontal{};
    float paddingVertical{};
    float entrySpacing{};
    float glyphGap{};
};

class TopBarMetrics {
  public:
    /// Row padding (iiSU mw5.l single-screen Row).
    static constexpr float rowPaddingTop = 8.0f;
    static constexpr float rowPaddingEnd = 8.0f;
    TopBarMetrics(float screenWidthDp, float screenHeightDp) noexcept;

    /// iiSU jj2.q0: the height of a two-row button prompt panel at a scale.
    [[nodiscard]] static float promptRowHeight(float scale) noexcept;

    [[nodiscard]] const StatusPillSizing& sizing() const noexcept {
        return sizing_;
    }
    [[nodiscard]] const HudStatusAlignment& alignment() const noexcept {
        return alignment_;
    }
    /// iiSU mw5.l: the default device class's status box x offset for a window aspect ratio.
    [[nodiscard]] float statusOffsetX(float aspect) const noexcept;
    /// iiSU nl2.d: clamp(1.22 hs7.d, 38, 64).
    [[nodiscard]] float profileSize() const noexcept;
    /// iiSU er3.w: the grid's top inset, dl3.i + 4, with the status pill shown (without it dl3.i
    /// is 84; iideck always shows it).
    [[nodiscard]] float gridTopInset() const noexcept;
    /// iiSU er3.x: the grid's bottom inset, dl3.j + 4, with the prompt row shown.
    [[nodiscard]] float gridBottomInset() const noexcept;
    /// iiSU a32.o for a clock string with or without letters.
    [[nodiscard]] StatusPillMetrics statusPill(bool clockHasLetters) const noexcept;
    /// iiSU mw5.h and jj2.b.
    [[nodiscard]] HintRowMetrics hintRow() const noexcept;

  private:
    /// iiSU dl3.a: the prompt panel scale from jj2.f0.
    float promptScale_{};
    StatusPillSizing sizing_;
    HudStatusAlignment alignment_;
};

} // namespace iideck::ui
