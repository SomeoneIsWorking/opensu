# Cancelling a Steam launch leaves the game running unseen

Status: open, not yet reproduced here.

The player: "I started a Steam game then cancelled it, it still launched in the background and I
could hear it but it didn't appear as running in openSU or anything so I ran the game again and
thankfully it just switched to the game."

Observed: B on the launch screen ends openSU's wait, but the launch Steam already accepted goes on;
the game runs with sound, not shown, not listed as running, no Close game. Playing it again finds
it and switches to it. Expected: cancel either stops the launch Steam is doing, or, once the game
has started, openSU treats it as running.

To reproduce: a Steam game launched from openSU, B at each launch stage (waiting for Steam,
updating, Steam's launch task, starting, loading); note at which stage the game still comes up.
