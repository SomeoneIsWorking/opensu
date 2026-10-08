// top_bar_metrics — sizes of iiSU's single-screen top bar, in dp.
//
// The is7/hs7 sizing objects and the status pill's own scales (home-grid.md §2).
// Pure arithmetic from the screen width in dp: nothing here draws.
#pragma once

#include <string_view>

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

/// The corner button prompt panels' sizes (iiSU mw5.h, mw5.k, jj2.b, jj2.g0, a32.b), in dp.
struct HintPanelMetrics {
    /// Left panel: BottomStart, start and bottom padding.
    float leftInset{};
    float leftBottom{};
    /// Right panel: BottomEnd, end and bottom padding.
    float rightInset{};
    float rightBottom{};
    float paddingHorizontal{};
    float paddingVertical{};
    float entrySpacing{};
    float glyphSize{};
    float glyphGap{};
    float cornerRadius{};
    /// Label em and line height in sp, letter spacing in em.
    float labelSize{};
    float labelLineHeight{};
    float labelTracking{};
    /// a32.b's text nudge before the key's own adjustment: Cal Sans's -1.8 t.
    float labelNudge{};
    /// jj2.g0's floor on the panel height.
    float minHeight{};

    /// iiSU jj2.g0: the panel height at a density, rounded to pixels term by term as Compose
    /// lays it out. Always two entries tall, whatever the panel holds.
    [[nodiscard]] float height(float density) const noexcept;
    /// iiSU a32.b: how far the label for `key` sits below the row's centre.
    [[nodiscard]] float labelShift(std::string_view key) const noexcept;
};

/// The centre title pill's sizes (iiSU jj2.c), in dp.
struct TitlePillMetrics {
    /// Below the row's own top padding (hs7.c).
    float topPadding{};
    float height{};
    float paddingHorizontal{};
    float minWidth{};
    /// The widest the text may run.
    float maxTextWidth{};
    /// titleMedium scaled, in sp.
    float fontSize{};
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
    /// iiSU jj2.c.
    [[nodiscard]] TitlePillMetrics titlePill() const noexcept;
    /// iiSU a32.e: a friend avatar's diameter, clamp(55 x 0.7 x 0.82, 24, 34).
    [[nodiscard]] static float avatarSize() noexcept;
    /// iiSU nl2.d: clamp(1.22 hs7.d, 38, 64).
    [[nodiscard]] float profileSize() const noexcept;
    /// iiSU er3.w: the grid's top inset, dl3.i + 4, with the status pill shown (without it dl3.i
    /// is 84; iideck always shows it).
    [[nodiscard]] float gridTopInset() const noexcept;
    /// iiSU er3.x: the grid's bottom inset, dl3.j + 4, with the prompt row shown.
    [[nodiscard]] float gridBottomInset() const noexcept;
    /// iiSU a32.o for a clock string with or without letters.
    [[nodiscard]] StatusPillMetrics statusPill(bool clockHasLetters) const noexcept;
    /// iiSU mw5.h, mw5.k and jj2.b.
    [[nodiscard]] HintPanelMetrics hintPanels() const noexcept;

  private:
    /// iiSU dl3 fields a, b, c (getters c(), a(), b()): the prompt panel, its text and its glyph
    /// scales.
    float promptScale_{};
    float textScale_{};
    float glyphScale_{};
    bool compact_{};
    float widthDp_{};
    StatusPillSizing sizing_;
    HudStatusAlignment alignment_;
};

} // namespace iideck::ui
