// panel_frame — what the side menus' layouts are given: the frame they sit in and the heights their
// heading and hints take, so no layout call has a run of bare floats.
#pragma once

namespace opensu::ui {

/// A `width` x `height` frame at `dp` pixels per dp.
struct PanelFrame {
    float width{};
    float height{};
    float dp{};
};

/// The heights a menu reserves: `heading` is the title line, `footer` the room the hints take.
struct PanelChrome {
    float heading{};
    float footer{};
};

} // namespace opensu::ui
