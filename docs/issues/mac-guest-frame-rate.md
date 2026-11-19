# The Mac guest shows about 45 of 60 frames

Seen 2026-11-19, MacBook Air M2 (MoltenVK 1.4.2) as guest of the Fedora host, Devoid over Wi-Fi,
PyroWave at 150-200 Mbit/s: `remoteVideo` after 30 s read decoded 1183, dropped 191, late 201,
drawn 1138; decode 8.0 ms p50 / 22.8 ms p95, submit 12.4 / 14.3 ms, capture to screen 28.6 / 45.7 ms.

Decode and submit on the guest take most of the frame. Next: measure the same session with
VideoToolbox HEVC on the Mac guest (no hardware decoder is built for macOS today) against
PyroWave on MoltenVK, and where submit's 12 ms goes (`GuestVideo`, the shell's present path).

A guest that receives no picture says nothing today: `remoteVideo` stays `codec=none` and the
launch card shows Loading. The guest should log when a session has drawn no picture for some
seconds, so a host that holds or loses video shows in the guest's log.
