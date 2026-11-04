# LAN remote play

Requested 2026-11-03: "I want LAN remote play, automatically show games in LAN from openSU,
seamlessly, Steam switched to Pyrowave, maybe we can use that too."

## What it does

- Every openSU on the LAN announces itself (mDNS/DNS-SD, `_opensu._tcp`) and serves its library
  on a LAN listener of its own (`lucent::http::Server`; never the loopback control channel). Another openSU shows those games in its own library,
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

1. Discovery and pairing, remote libraries listed (no streaming). Built on Linux
   (`docs/design/lan.md`): `_opensu._tcp` over Avahi, pairing with a six-digit code shown on both
   screens and a per-peer secret, signed requests on a LAN listener of its own (not the control
   channel), each paired openSU's installed games and covers as a launcher in Library, Remove
   revoking both sides. Shown with three hidden openSUs on one machine. Discovery on Windows,
   macOS and Android says it is not built yet.
2. Launch on the host, PyroWave video plus Opus audio over UDP, pad and keyboard input back.
   Built on Linux (`docs/design/lan.md#remote-play`): `/lan/play` with per-session HKDF keys, each
   datagram sealed with its counter as nonce and replays dropped; the host's game launched through
   its normal path inside its Gamescope, Gamescope's PipeWire stream coded with PyroWave (pinned
   fork, intra only, a lost frame dropped) and the game's own null sink with Opus; the guest's pad
   on uinput and keys and pointer through XTest; the guest shows it full screen, Guide opens its own
   menu with End remote play. Shown with two hidden openSUs on one machine over Avahi on a Game Boy
   test ROM in a headless Gamescope.
3. Fallback encoder and bandwidth probing. Built on Linux (`docs/design/lan.md#remote-play`):
   H.264, HEVC and AV1 through libavcodec on VA-API both ways, key frames asked for after a loss;
   the codec and rate per link from an opening probe burst and the guest's reports each second,
   PyroWave on links that carry 100 Mbit/s with room and a hardware codec below, re-probed while
   on one; analogue triggers, mouse buttons held and the wheel; the guest's game mapping played by
   the host's remote pad. Shown with two hidden openSUs over Avahi on a full-screen moving Game Boy
   ROM, the switch forced by a link cap on the guest (`POST /remote/link`).
4. Windows, macOS and Android guests and hosts; the guest decoding straight into the presenter's
   image (needs Vulkan interop SDL GPU does not offer); hardware codecs in the AppImage.
