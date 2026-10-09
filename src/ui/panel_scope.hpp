// panel_scope — draws what follows scaled about a point for as long as it lives, as a panel
// grows in or shrinks out.
#pragma once

#include "raylib.h"
#include "rlgl.h"

namespace opensu::ui {

class PanelScope {
  public:
    PanelScope(Vector2 about, float scale) : active_{scale != 1.0f} {
        if (active_) {
            rlPushMatrix();
            rlTranslatef(about.x, about.y, 0.0f);
            rlScalef(scale, scale, 1.0f);
            rlTranslatef(-about.x, -about.y, 0.0f);
        }
    }
    ~PanelScope() {
        if (active_) {
            rlPopMatrix();
        }
    }

    PanelScope(const PanelScope&) = delete;
    PanelScope& operator=(const PanelScope&) = delete;

  private:
    bool active_;
};

} // namespace opensu::ui
