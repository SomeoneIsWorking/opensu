# fedora sounds busy with nothing running

Status: open, not investigated.

Reported by the player (2026-11-25): "fedora sounds like it is doing heavy CPU use even though no
agents are working on it right now". Reported with a remote play session from fedora to the Ally,
so openSU's host side (capture, encode, the nested Gamescope) is the first suspect.

## Next

With openSU idle on Home and again while hosting, sample per-process CPU (`pidstat -u 5`) and
openSU's threads (`top -H -p`), and compare with openSU closed.
