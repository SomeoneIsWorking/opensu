# Cancelling a Steam launch leaves the game running unseen

Status: fixed in the tree, not committed.

The player: "I started a Steam game then cancelled it, it still launched in the background and I
could hear it but it didn't appear as running in openSU or anything so I ran the game again and
thankfully it just switched to the game."

Observed: B on the launch screen ends openSU's wait, but the launch Steam already accepted goes on;
the game runs with sound, not shown, not listed as running, no Close game. Playing it again finds
it and switches to it. Expected: cancel either stops the launch Steam is doing, or, once the game
has started, openSU treats it as running.

Reproduced 2026-10-21, `scratch/steam-cancel/tools/cancel_repro.py agent-cancel before 4440600`:
hidden openSU (`--hidden --steam`) in the headless Gamescope fork, Managed mode, the player's Steam
and login, Hamsteria Demo, B at each stage (`logs/before.txt`, `shots/before-*.png`).

| B during | Before the fix | After the fix |
| --- | --- | --- |
| Waiting for Steam | no launch | no launch |
| Starting (`steam -applaunch` running) | game up 3.6 s later, untracked | Steam's action cancelled, no game |
| Steam's launch task (Synchronizing cloud) | game up 1.8 s later, untracked | Steam's action cancelled, no game |
| Loading (reaper up, no window) | game killed | game killed, Steam lets go |
| Updating | not reproduced: no update pending | not reproduced |

Before, after the stray game came up: `launching=false`, `inGame=false`, the quick menu without
Close game (`shots/before-preparing-quick-menu.png`).

Cause: `launch/handoff.cpp` `Handoff::start` ended a cancelled Steam launch by SIGKILLing the
processes matching `AppId=<id>` that existed at that moment and returning, while the launch action
Steam had accepted (cloud sync, stats, controller config) went on and started the reaper after
openSU had stopped watching.

Fix: a cancel ends the launch, it does not adopt it. The player pressed B to not play, so a game
Steam starts regardless is ended, not shown. `Handoff::endSteamLaunch` (also the Close game path
for Steam games) asks Steam to drop each unfinished action of this launch through
`SteamClient.Apps.CancelLaunch(<appid>)`, the call behind the Cancel on Steam's library Play button
(`sp.js`; `CancelGameAction(<action id>)` is its dialogs' cancel), SIGKILLs the game's tree whenever
it shows, and returns once Steam has taken the request, has no action of it pending, does not say
the game runs and no process of it remains, bounded by `Handoff::startTimeout`. The launch panel
reads "Cancelling" meanwhile (`LaunchProgress::Stage::Cancelling`).

Proof after the fix: `logs/after.txt`, `logs/after-close.txt`, `shots/after-*.png`,
`shots/after-close-*.png`: `[steam] cancelled the launch of 4440600`, Steam's action `ended`, no
`AppId=4440600` process 40 s after B, `shots/after-close-starting-after-b-0.7s.png` (Cancelling);
Close game from the quick menu over the running game ends it and returns the shell
(`Steam let go of Hamsteria Demo`). No Gamescope screenshot was taken.

On B the panel drops its "B Cancel" hint and takes no more presses (`PanelFlow`, `Use::Cancelling`),
and the launch ends with the toast "<title> launch cancelled" (`GameEnd::cancelled`, set by
`GameLaunch::cancel`), not "<title> closed": `shots/after-ui-*-after-b-0.7s.png`,
`shots/after-toast-starting-after-B-toast.png`, `logs/after-ui.txt`, `logs/after-toast.txt`.

A launch Steam never starts: `steam -applaunch 1` (an id nobody owns) leaves nothing in Steam's
record, neither an action nor `GetActiveGameActions` nor `GetGameActionForApp` (`[0,0,""]`), for
12 s (`logs/explore.txt`, e2), which is also what the record shows in the moment before Steam starts
an action it did accept. The record alone cannot tell "not yet" from "never", so the cancel still
waits on `Handoff::startTimeout` in that case. `SteamClient.Apps.RunGame("1", "", -1, 100)` instead
starts an action that ends `Failed` ("Game configuration unavailable"), and its action appears
after the call returns, not within it (`logs/explore2.txt`, e4). Proper fix, not done: launch
through `RunGame` where openSU owns Steam (Managed, Isolated), so every request gets an action that
ends; Normal keeps `-applaunch`. Open: the meaning of RunGame's fourth argument (the library passes
its own constant).

Unproven: B while Steam updates the game first.
