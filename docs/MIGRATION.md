# Migration Status

Tracks progress of `docs/DESIGN.md`'s refactor, on branch `refactor/cpp-design` (branched from
`dev/minimal`). `dev/minimal` remains the behavioral reference throughout — nothing here is
claimed as done until it has test parity against that branch.

This document favors honesty over completeness: a step is listed as done only once a Make target
actually exercises it, and the hardware phase is never described as "verification remaining" for
code that does not exist yet.

## Done

- `refactor/cpp-design` branch created off `dev/minimal`.
- `docs/DESIGN.md` adopted (adapted from an external architecture prototype, re-targeted at this
  repo's actual module layout).
- `Makefile` scaffolding for the new tree (`build/reader`, `build/simulator_gui`,
  `build/simulator_test`, `build/epub_test`, `test`, `cpp-all`, `cpp-gui`, `format`,
  `format-check`) added alongside the untouched `dev/minimal`-era targets (`build/xr_sim`,
  `build/xr_m5paper_gui`, `test-epub`, `test-title`, `run`), which continue to pass unmodified.
- `.clang-format` (Google style, adapted) added at repo root.

## In progress / not started

Everything else in `docs/DESIGN.md`'s refactor order (§30) is not started: `core/`, `reader/`,
`app/*.hpp`, `boards/sim/`, the autonomous input-injection test suite, font-asset re-targeting,
the real `secret::store` implementation, `drivers/it8951`+`drivers/gt911` extraction,
`boards/m5paper/`, build-time board selection, the Xteink simulator profile, and old-tree
retirement.

## Hardware-target verification

No `boards/m5paper/` or `drivers/` code exists yet in this tree (it is ported from
`port/m5paper/` later in the plan, not before). Once it does, this section will state plainly
that it is **simulator-verified only** until real-hardware verification happens — this sandbox
has no ESP-IDF toolchain available, so `idf.py build`/`idf.py flash monitor` and the manual
behavioral checklist (rotation correctness, page-turn latency, touch at both I2C addresses,
battery percentage sanity, sleep/wake via both ext0 and timer wakeup) must be run by the user on
real M5Paper hardware before any hardware-board claim in this document is upgraded from
"ported" to "verified."

## Explicitly out of scope for this pass

- Lua application bindings and hot reload (`docs/DESIGN.md` §24/§25).
- A real Xteink hardware board (no driver exists anywhere today, old or new tree); only a
  simulator-only logical profile is planned.
