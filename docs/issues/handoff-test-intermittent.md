# handoff test fails intermittently under load

Status: open.

`tests/launch` `handoff` failed once in a full `ctest -j8` on `build/clang` (12.8 s, where a pass
takes about 48 s), at 341693a, which does not touch the handoff. Five runs alone and two more full
runs passed. The failing run's output was not kept; the next failure needs
`ctest -R '^handoff$' --output-on-failure` (or the full suite with it) to name the failing check
before anything changes.

2026-10-22, `build/agent-fgc` (frame generation settings, not touching the handoff): `handoff`
timed out in a full `ctest -j8`, then passed alone in 59.8 s. That is a timeout, not a failed check.

2026-10-22, `build/agent-crash` (the `GameLaunch` thread fix): not the cause of the SIGTERM crash
(`shell-sigterm-crash-with-game.md`). `handoff_test` never uses `GameLaunch`; each test's
`Handoff` is a local that outlives the `std::async` future running `start()`, and the future joins
in its destructor, so no thread outlives what it reads. A use after free would also show as a
crash, not a timeout. Four full `ctest -j8` runs passed `handoff` in 59.7–65.9 s
(`scratch/sigterm-crash/ctest/full-1.log` … `full-4.log`), so the timeout did not recur. A
candidate for the early failure (12.8 s): `launch-ends-as-it-starts.md`, a launch whose game
counts as gone on its first phase-two check; unconfirmed.
