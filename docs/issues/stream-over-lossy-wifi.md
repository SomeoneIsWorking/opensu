# Remote play over a lossy Wi-Fi link

Status: open. Seen 2026-11-06 with a Huawei tablet on 2.4 GHz (Wi-Fi 6, RSSI -50) and a wired host.

The link loses packets at any rate: 4.3 % of 1500 pings at 100 a second during a stream, round
trips up to 382 ms, with no UDP receive buffer errors on the tablet. openSU's reports show 1 to 2 %
of datagrams lost, and the same share of audio packets, with the low-latency Wi-Fi lock held.

Two things follow in `LinkRate` and the predicted codecs:

- `LinkRate` reads that loss as congestion and steps the plan down until it stops at the floor:
  HEVC went from 40 to 2 Mbit/s and stayed there, on a test ROM that needs little. A real game at
  2 Mbit/s would be badly blocky, though the link carried 40 Mbit/s with 0.2 % lost a minute before.
- Each lost packet costs the frames until the next key frame: 8 of about 58 frames a second were
  dropped on that link.

What would fix it: tell loss that does not grow with the rate from loss that does, in `LinkRate`,
and mend a lost packet without a key frame (forward error correction per frame, or intra refresh
on the host's encoder). Not tried: the same tablet on 5 GHz.
