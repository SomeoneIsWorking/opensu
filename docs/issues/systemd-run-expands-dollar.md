# Game arguments containing `$` are rewritten at launch

Status: open.

`launch/instance.cpp:Instance::start` runs games through `systemd-run`, which expands `$VAR` and
`${...}` in the command line it is given. An argument holding `$` (a path, a Wine argument, a
plugin's command) reaches the game changed. Found while testing the overlay fix (commit after
4571ccb). Fix at `Instance::start`: pass `$` through escaped as `$$`, the form `systemd-run`
documents, and show a launch with a `$` in a ROM path arriving unchanged.
