# Game arguments containing `$` are rewritten at launch

Status: closed.

`launch/instance.cpp:Instance::start` runs games through `systemd-run --scope`. From systemd 258
`--scope` expands environment variables in the command line by default (`src/run/run.c`:
`replace_env_argv` over the whole `arg_cmdline`, program path included, before `execvpe`): a word
that is exactly `$NAME` is replaced by the variable split into words, `${NAME}` anywhere in a word
is replaced, and `$$` becomes `$`. A `$NAME` inside a word is left alone. systemd 254 to 257 do not
expand in `--scope` mode by default (systemd 254's NEWS asks `--scope` callers passing `$` to say
`--expand-environment=yes/no` explicitly), and 253 and older never do.

Escaping `$` as `$$` would be wrong on 254 to 257: the `$$` reaches the game unchanged. The fix
passes `--expand-environment=no` (systemd 254 and later; the AppImage's floor, Ubuntu 24.04, has
255), so the command is passed through as given on every supported systemd.

Reproduced on systemd 259 (`scratch/dollar/runs/dollar-before/shell.log`): the ROM
`Kirby $HOME ${X} (Europe).gba` reached opensu-retro as `Kirby $HOME  (Europe).gba` and failed to
load. After the fix (`scratch/dollar/runs/dollar-after/`), `/proc/<runner pid>/cmdline` holds the
name unchanged, the runner's `/status` frames rise (189, 248, 308) and Close game ends it.
`instance_test` starts a program whose path and arguments hold `$HOME`, `${HOME}`, `$$` and `$`
and checks they arrive unchanged.
