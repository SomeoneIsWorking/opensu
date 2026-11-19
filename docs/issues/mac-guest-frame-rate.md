# A Mac guest's Wi-Fi stalls cost latency

Seen 2026-11-19, MacBook Air M2 as guest of the Fedora host, Devoid over Wi-Fi, HEVC on
VideoToolbox at 1782x1080: decoded about 600 per 10 s, drawn about 495, capture to screen 23 ms
p50, with the host's sends even (16.7 ms apart).

## Cause

The Mac's link stalls 2 to 4 times a second for 28 to 95 ms, whatever is sent: a frame after a
stall arrives 28 ms median from its send against 5 ms otherwise, and the frames behind it come in
one burst, of which the loop shows the newest (`docs/design/lan.md`, the guest's frame loop).
Marking the stream as video (DSCP AF41, `NET_SERVICE_TYPE_VI`) cut the usual send-to-receive by a
third and left the stalls as they were. The pattern is macOS's AWDL interface (AirDrop, Handoff)
hopping channels: with AirDrop off the late arrivals went from 7.3 a second to none.

## What is done

- The guest holds frames by the stalls' length (`stream::PlayoutDelay`), on every link: repeats
  from 11.6 a second to 0.8, every decoded frame drawn, at about 111 ms from capture to screen against 26 (b1d26da1, 2026-11-19).
- A guest's lower frame rate now reaches the host (`stream::FramePace`). At 45 a second the stalls
  are as many per frame as at 60 (1.5 against 1.9 a second, p99 91 ms), so a lower rate does not
  cure them.
- With AirDrop off the link measures over PyroWave's floor, and PyroWave's 417 KB frames took
  14 ms on the wire and 13 to 35 ms to decode on the Mac, 57 ms from send to taken. A guest now
  gives PyroWave up when its frames are decoded over 25 ms after their send (`stream::GuestFit`):
  HEVC from 5 s in, no repeats after, 20 ms from send to taken and 26 from capture to screen
  (c481139c, 2026-11-19).

## Open

- Each bitrate step over 20 % reopens the host's VA-API encoder, since FFmpeg fixes its rate
  control at open (`hardware_codec_vaapi.cpp`, `reopenShare`); the key frame after carries new
  parameter sets, VideoToolbox will not take them into its session and builds a new one, about
  20 ms of decode once. Changing the rate in place needs the encoder's VA rate-control parameters
  set per sequence, which FFmpeg does not expose.
