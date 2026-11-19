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
hopping channels; the player keeps those on.

## What is done

- The guest holds frames by the stalls' length (`stream::PlayoutDelay`), on every link: repeats
  from 11.6 a second to 1.6, at about 110 ms from capture to screen against 26 (2026-11-19).
- A guest's lower frame rate now reaches the host (`stream::FramePace`). At 45 a second the stalls
  are as many per frame as at 60 (1.5 against 1.9 a second, p99 91 ms), so a lower rate does not
  cure them.

## Open

- Each bitrate step over 20 % reopens the host's VA-API encoder, since FFmpeg fixes its rate
  control at open (`hardware_codec_vaapi.cpp`, `reopenShare`); the key frame after carries new
  parameter sets, VideoToolbox will not take them into its session and builds a new one, about
  20 ms of decode once. Changing the rate in place needs the encoder's VA rate-control parameters
  set per sequence, which FFmpeg does not expose.
