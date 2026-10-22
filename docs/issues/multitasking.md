# Several things open at once: browse, launch, switch

Status: open, planned.

The player: "allow browsing the library and launching another game while a game is running, maybe
we can add suspending for example. Or just making minimized game silent and switching between open
things."

Decided:
- While a game runs, Guide opens the full shell (library, search, details), not only the quick menu.
- Launching another game leaves the first one open in the background. What happens to a background
  game is a layered setting, "In the background":
  - Pause (default for ROM games): the runner pauses itself over its control channel (no frames,
    no audio, state kept); a standalone emulator's scope is frozen with the cgroup freezer
    (`systemctl --user freeze`), thawed when it comes back.
  - Keep running, muted (default for Steam and store games, which online play or a launcher's
    watchdog may not survive frozen): its PipeWire streams muted, still drawing.
- A switcher (Guide double-press, and a row in the quick menu) lists the open games and apps with
  their state; picking one brings it in front, thawed or resumed and unmuted.
- Close game closes the one in front; quitting openSU closes them all (each the normal way).
- Owners: the running-game owner becomes a list of open things with one in front; focus follow,
  pad ownership, frame generation and the overlay act on the one in front.

To find out: whether Gamescope keeps a frozen client's last frame or drops its window; memory with
two GPU cores loaded; how Steam treats a second game launch while one runs (it refuses some).
