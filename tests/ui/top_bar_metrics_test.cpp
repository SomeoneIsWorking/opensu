// TopBarMetrics against iiSU's is7/hs7/a32.o numbers (docs/reference/iisu/home-grid.md §2).
// Expected values are worked by hand from the spec's formulas.
#include "top_bar_metrics.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::near;
using opensu::ui::StatusPillMetrics;
using opensu::ui::TopBarMetrics;

void phone640() {
    const TopBarMetrics m{640.0f, 360.0f};
    near(m.sizing().scale, 0.94542, "640: is7.a");
    near(m.sizing().stretch, 0.71861, "640: is7.b");
    near(m.sizing().endPadding, 12.74576, "640: is7.c");
    near(m.alignment().statusPillHeight, 35.39666, "640: hs7.d");
    near(m.alignment().statusTopPadding, 7.37431, "640: hs7.c");
    near(m.alignment().profileTopPadding, 2.07264, "640: hs7.a");
    near(m.alignment().compactWidthScale, 0.66415, "640: hs7.b");
    near(m.profileSize(), 43.18393, "640: profile button");
    near(m.gridTopInset(), 61.6646, "640: grid top inset");
    const StatusPillMetrics pill = m.statusPill(false);
    near(pill.height, 35.39666, "640: pill height");
    near(pill.width, 192.86644, "640: 24 h pill width");
    near(m.statusPill(true).width, 215.55661, "640: 12 h pill width");
    near(pill.contentScale, 0.67939, "640: content scale");
    near(pill.fontSize, 10.87026, "640: clock font");
    near(m.statusOffsetX(1.6f), 3.9774, "640: status offset on 16:10");
}

void reference853() {
    const TopBarMetrics m{853.0f, 480.0f};
    near(m.sizing().scale, 1.10, "853: is7.a");
    near(m.sizing().stretch, 0.85, "853: is7.b");
    near(m.sizing().endPadding, 16.0, "853: is7.c");
    near(m.alignment().statusPillHeight, 41.184, "853: hs7.d");
    near(m.alignment().statusTopPadding, 8.58, "853: hs7.c");
    near(m.alignment().profileTopPadding, 6.172, "853: hs7.a");
    near(m.alignment().compactWidthScale, 0.57082, "853: hs7.b");
    near(m.profileSize(), 50.24448, "853: profile button");
    near(m.gridTopInset(), 69.29424, "853: grid top inset, 156 px in the capture");
    near(m.gridBottomInset(), 71.6176, "853 x 480: grid bottom inset");
    const StatusPillMetrics pill = m.statusPill(false);
    near(pill.height, 41.184, "853: pill height");
    near(pill.width, 224.4, "853: 24 h pill width");
    near(pill.contentScale, 0.935, "853: content scale");
    near(pill.paddingHorizontal, 9.35, "853: the text row's end padding");
    near(pill.fontSize, 14.96, "853: clock font");
    near(pill.batteryIcon, 25.245, "853: battery icon");
    const StatusPillMetrics twelve = m.statusPill(true);
    near(twelve.width, 250.8, "853: 12 h pill width");
    near(m.statusOffsetX(1.6f), 4.0, "853: status offset on 16:10");
    near(m.statusOffsetX(4.0f / 3.0f), 10.0, "853: status offset on 4:3");
}

void promptRow() {
    near(TopBarMetrics::promptRowHeight(1.0f), 60.0, "scale 1: 2 x 22 + 2 x 7 + 2");
    // The emulator capture: 853 x 456 dp after its status bar; grid bottom 155.1 px at 2.25.
    near(TopBarMetrics{853.0f, 456.0f}.gridBottomInset(), 68.93672, "853 x 456: bottom inset");
    // 640 x 360 is not compact (max side over 560): a = clamp(0.92 x 0.75 x 0.94, 0.68, 1).
    near(TopBarMetrics{640.0f, 360.0f}.gridBottomInset(), 60.24, "640 x 360: bottom inset");
}

void hintPanels() {
    // The emulator capture's 853 x 456 dp: dl3 fields a = 0.82156, b = 0.8789, c = 0.79101.
    const opensu::ui::HintPanelMetrics p = TopBarMetrics{853.0f, 456.0f}.hintPanels();
    near(p.paddingHorizontal, 8.2156, "hint panel: horizontal padding 10 a");
    near(p.paddingVertical, 6.57248, "hint panel: vertical padding 8 a");
    near(p.entrySpacing, 1.64312, "hint panel: row spacing 2 a");
    near(p.glyphSize, 17.40222, "hint panel: glyph 22 c");
    near(p.glyphGap, 3.5156, "hint panel: glyph gap 4 b");
    near(p.labelSize, 14.0624, "label: titleMedium 16 sp x b");
    near(p.labelLineHeight, 20.95880, "label line: 24 x 1.08 x b x 0.92");
    near(p.labelNudge, -1.58202, "Cal Sans nudge: -1.8 b");
    near(p.minHeight, 50.69227, "hint panel: jj2.g0 floor");
    // At 2.25 px/dp: 4 + 2 max(39, 0 + 48) + 2 x 15 = 130 px, the captured panel.
    near(p.height(2.25f), 130.0 / 2.25, "panel height matches the capture");
    near(p.labelShift("B"), -0.08202, "B label sits 0.08 dp high");
    near(p.labelShift("-"), -0.73202, "- label sits 0.73 dp high");
    near(p.leftInset, 8.0, "left panel 8 dp from the start");
    near(p.rightBottom, 4.0, "right panel 4 dp from the bottom");
}

void wide1280() {
    // t saturates at 832 dp, so 1280 dp sizes as 853 does.
    const TopBarMetrics m{1280.0f, 800.0f};
    near(m.sizing().scale, 1.10, "1280: is7.a");
    near(m.alignment().statusPillHeight, 41.184, "1280: hs7.d");
    near(m.alignment().statusTopPadding, 8.58, "1280: hs7.c");
    near(m.gridTopInset(), 69.29424, "1280: grid top inset");
    near(m.statusPill(false).width, 224.4, "1280: 24 h pill width");
}

void titlePill() {
    // f = 0.9 + 0.1 clamp((w - 538) / 294): 0.93469 at 640, 1 from 832 up.
    const opensu::ui::TitlePillMetrics phone = TopBarMetrics{640.0f, 360.0f}.titlePill();
    near(phone.height, 44.86531, "640: title pill height");
    near(phone.paddingHorizontal, 20.56327, "640: title pill padding");
    near(phone.minWidth, 130.85714, "640: title pill min width");
    near(phone.maxTextWidth, 478.87347, "640: title text max width");
    near(phone.fontSize, 14.95510, "640: title font");
    near(phone.topPadding, 7.37431, "640: title pill sits at hs7.c");
    const opensu::ui::TitlePillMetrics wide = TopBarMetrics{853.0f, 480.0f}.titlePill();
    near(wide.height, 48.0, "853: title pill height");
    near(wide.minWidth, 140.0, "853: title pill min width");
    near(wide.fontSize, 16.0, "853: title font");
    near(TopBarMetrics::avatarSize(), 31.570, "a32.e avatar");
}

} // namespace

int main() {
    titlePill();
    phone640();
    reference853();
    wide1280();
    promptRow();
    hintPanels();
    std::printf("top_bar_metrics: all checks passed\n");
    return 0;
}
