# Home covers can stay on the loading placeholder

Status: open; cause not traced.

Observed: in one of three `tools.screenshots --parity` runs of the same build (`scratch/parity/ref-c`),
the Teeworlds and SuperTux covers on Home kept the loading placeholder for the whole run, although
the log reports every image decoded. The other two runs show the covers.

Expected: every decoded cover reaches its tile.

Reproduce: repeat `uv run --frozen python -m tools.screenshots --parity scratch/parity/<run>` and
`python -m tools.screenshots.compare scratch/parity/ref-a scratch/parity/<run>`; a failing run fails
`home` (mean about 5.8, 4.9% of pixels over 16). Nothing in `/state` reports pending artwork, so the
tool cannot wait for it; a state field for outstanding artwork would let a test assert on it.
