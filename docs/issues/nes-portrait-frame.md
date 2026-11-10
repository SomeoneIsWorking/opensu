# The NES visual preset has no portrait art of its own

Status: open.

Auto Console on a NES game frames the picture with common-overlays'
`borders/animated-border/img-nes/background.png` (1920x1080). On a tall screen the same landscape
art is letterboxed above the touch controls, so the frame is a thin band around a small picture,
where a portrait layout would stand the controller art below or around a full-width picture.

No logo-free portrait NES art under a licence that allows download was found: overlay-borders'
NES border shows Nintendo's logo, and the other candidates show Nintendo characters
(`docs/prior-art.md`, Visual preset frames).

Fix: find or draw portrait NES art (logo-free, CC BY or more open), pin it in
`assets/visual-presets.json` as the `nes` preset's `portrait` with its picture rect.
