# LAN remote play

Requested 2026-11-03: "I want LAN remote play, automatically show games in LAN from openSU,
seamlessly, Steam switched to Pyrowave, maybe we can use that too."

## What it does

- Every openSU on the LAN announces itself (mDNS/DNS-SD, `_opensu._tcp`) and its library over its
  control channel (`lucent::http::Server`). Another openSU shows those games in its own library,
  marked with the host's name, without setup beyond accepting the host once (a pairing code shown
  on both screens).
- Starting a remote game starts it on the host through the host's own launch path, captures the
  host's output (Gamescope's PipeWire stream on Linux), encodes it, and streams video, audio and
  input both ways. The guest shows it full screen; the Guide still opens the guest's menu.
- Steam games can also go through Steam's own Remote Play where both ends run Steam; openSU's own
  stream covers everything else (ROMs, GOG, Epic, own games).

## Video codec

PyroWave (Hans-Kristian Arntzen, MIT, github.com/Themaister/pyrowave): a Vulkan compute wavelet
codec built for latency, now in Steam's beta Remote Play. It needs about 125–300 Mbit/s and wired
gigabit; over Wi-Fi a conventional encoder (Vulkan Video or VA-API H.264/HEVC/AV1) is the fallback.
The codec is picked per link from measured bandwidth. PyroWave would be vendored through a pinned
fork as any third-party code.

## Order

1. Discovery and pairing, remote libraries listed (no streaming).
2. Launch on the host, PyroWave video plus Opus audio over UDP, pad and keyboard input back.
3. Fallback encoder, bandwidth probing, Windows/macOS/Android guests.
