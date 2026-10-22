# handoff test fails intermittently under load

Status: open.

`tests/launch` `handoff` failed once in a full `ctest -j8` on `build/clang` (12.8 s, where a pass
takes about 48 s), at 341693a, which does not touch the handoff. Five runs alone and two more full
runs passed. The failing run's output was not kept; the next failure needs
`ctest -R '^handoff$' --output-on-failure` (or the full suite with it) to name the failing check
before anything changes.

2026-10-22, `build/agent-fgc` (frame generation settings, not touching the handoff): `handoff`
timed out in a full `ctest -j8`, then passed alone in 59.8 s. That is a timeout, not a failed check.
