# A Gamescope convar set from the environment skips its callback

Status: open; worked around in tools by a script on `GAMESCOPE_SCRIPT_PATH`.

Observed: `gamescope_log_framegen=debug` in Gamescope's environment logs `Overriding from
environment variable: gamescope.convars.log_framegen.value = debug`, but the framegen log stays at
its default level and no `show ... t=` lines appear (fork branch `opensu`, the same code at `a04d32dd`).

Cause: `main.cpp` runs each `gamescope_<name>=<value>` as the Lua `gamescope.convars.<name>.value =
<value>`, and `convar_script.cpp` binds `value` straight to `ConVar<T>::m_Value`, so the write
bypasses `ConVar::SetValue` and the convar's callback (the log level's setter) never runs.

Workaround: a script directory on `GAMESCOPE_SCRIPT_PATH` (after the default
`/usr/local/share/gamescope/scripts:/etc/gamescope/scripts`) with a `.lua` file holding
`gamescope.convars.log_framegen:call("debug")`, which goes through `CallWithArgString` and runs
the callback.

Proper fix: bind `value` in `convar_script.cpp` as a property whose setter calls `SetValue`; a
fork change, not made yet.
