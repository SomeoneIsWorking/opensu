// touch_layer — on-screen controls for a touchscreen with no controller, the runner's and the ones
// over a remote game. It runs a `ControlPlan`, the generic pad (`pad_layout`) unless it is given an
// arcade machine's panel (`arcade_layout`): each finger keeps the control it went down on, so
// several are held at once and a thumb slides within one; the buttons held reach the game as a
// pad's do, and the pause button holds Guide, which asks for the shell's menu as a pad's Guide
// does. The pieces are composed into an overlay the size of the surface.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "gamepad/event.hpp"
#include "touch/arcade_layout.hpp"
#include "touch/label_face.hpp"
#include "touch/touch_art.hpp"
#include "touch/touch_controls.hpp"
#include "touch/touch_surface.hpp"

namespace opensu::touch {

/// Where a control is on the target: a stick (`button` None, how far it reaches) or a button.
struct ControlPlace {
    gamepad::Button button{gamepad::Button::None};
    float x{0.0F};
    float y{0.0F};
    float radius{0.0F};
};

class TouchLayer {
  public:
    /// The art is read from `art` (assets/touch).
    explicit TouchLayer(std::filesystem::path art);

    /// Shows `panel` instead of the pad from the next `layout`.
    void setPanel(ArcadePanel panel);
    /// Draws the controls at `size` times their size from the next `layout`, as far as they fit.
    void setSize(float size);
    /// Places the controls on `surface`, a panel around the frame at `picture`; fingers down are
    /// let go.
    void layout(const WindowSurface& surface, const Box& picture = {});
    /// Where the controls are now.
    [[nodiscard]] std::vector<ControlPlace> places() const;

    /// Takes a finger; reports whether the buttons held changed.
    bool finger(const Finger& finger);
    /// Lets every finger go; reports whether any button was held.
    bool releaseAll();
    /// The buttons the fingers hold now.
    [[nodiscard]] std::vector<gamepad::Button> held() const;
    /// Whether a finger holds `button`.
    [[nodiscard]] bool pressed(gamepad::Button button) const;

    /// The controls as an overlay the size of the surface, drawn again when what is held changed.
    [[nodiscard]] OverlayImage image();

  private:
    struct Touch {
        std::size_t control{0};
        std::vector<gamepad::Button> buttons;
    };

    /// The index of the control a finger at `x`, `y` takes.
    [[nodiscard]] std::optional<std::size_t> controlAt(float x, float y) const;
    [[nodiscard]] std::vector<gamepad::Button> buttonsAt(const Control& control, float x,
                                                         float y) const;
    /// How far a piece leaning `distance` moves on x and y for the directions held.
    [[nodiscard]] std::pair<float, float> leaning(float distance) const;
    void draw();

    std::filesystem::path fontFile_;
    ArtSource art_;
    std::optional<ArcadePanel> panel_;
    /// Made for a panel: the pad has no words.
    std::unique_ptr<LabelFace> labels_;
    ControlPlan plan_;
    float size_{1.0F};
    unsigned width_{0};
    unsigned height_{0};
    std::map<std::uint64_t, Touch> touches_;
    std::vector<unsigned char> pixels_;
    bool dirty_{true};
};

} // namespace opensu::touch
