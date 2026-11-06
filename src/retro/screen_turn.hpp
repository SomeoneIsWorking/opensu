// screen_turn — Rotate screen in a running game, the one owner of whether it is on and the one way
// to change it: the touch button, the key, the shell's Guide row (`POST /turn`) and the game's
// setting (`POST /video`) all come here. Where `shell_handoff` is active the Activity stands in
// portrait, the shell's with it; elsewhere the picture is turned for a rotated monitor.
// A press is left in the control file, from where the shell keeps it as the game's setting.
#pragma once

namespace opensu::retro {

class ControlServer;
class Presenter;

class ScreenTurn {
  public:
    /// Starts as the game's setting `turned` says, which is no press. `control` may be null.
    ScreenTurn(Presenter& presenter, ControlServer* control, bool turned);

    /// Frame loop only, like `toggle`.
    void set(bool turned);
    void toggle() {
        set(!turned_);
    }

  private:
    void apply() const;

    Presenter& presenter_;
    ControlServer* control_;
    bool turned_;
};

} // namespace opensu::retro
