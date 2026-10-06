# ZIP Architecture Adoption Plan

This document governs adoption of `/home/dao/data/reader-wayland-simulator-final.zip` into this
repository. The ZIP is an architectural base, not a replacement for the working product tree.

## Objective

Adopt the ZIP's layered architecture and Lua HMI without losing any behavior, visual design,
performance work, hardware support, or validation already achieved in this repository.

The target relationship is:

```text
ZIP architecture and runtimes
        +
current reader behavior, GUI design, optimizations, and M5Paper port
        =
one validated C++/Lua ebook-reader tree
```

Wayland is the default host GUI after adoption. The M5Paper board remains a first-class target and
is never treated as a simulator-only concern.

Development is simulator-first: Wayland is the primary GUI development and comparison environment.
M5Paper porting and flashing resume only after the new native/Lua GUI matches the current GUI through
captured framebuffer checkpoints and input scenarios.

## Source Of Truth

Each source owns a different kind of truth:

| Area | Source of truth |
| --- | --- |
| Layering, Lua runtime, routing, widgets | ZIP bundle |
| Current product behavior | Current workspace native implementation |
| GUI geometry, typography, colors, focus, dialogs | Current workspace GUI and framebuffer captures |
| EPUB correctness and pagination | Current workspace tests and real EPUB behavior |
| M5Paper drivers, pins, power, SD, touch, panel | Current workspace `boards/m5paper/` and `drivers/` |
| Refresh/performance behavior | Current workspace hardware observations and logs |
| Lua product behavior | Native C++ app after parity is established |

When sources disagree, do not silently choose one. Record the conflict, add a regression case, and
resolve it deliberately.

## Non-Negotiable Rules

1. Do not overwrite the current workspace with the ZIP.
2. Perform the migration in a separate branch/worktree until parity gates pass.
3. Preserve the current M5Paper board and driver implementation as the hardware reference.
4. Preserve the current native C++ app as the product behavior reference until Lua parity passes.
5. Do not remove a current feature without an equivalent replacement and a passing regression test.
6. Do not change GUI geometry or interaction semantics as part of an architectural move.
7. Keep board and driver names out of `core/`, `reader/`, and application policy.
8. Keep Lua dependent on stable native bindings; never move EPUB, storage, display, or hardware
   implementation into Lua.
9. Keep Wayland presentation code separate from page layout and product behavior.
10. Do not flash hardware from a tree that has not passed the host and ESP-IDF build gates.
11. Preserve rollback to the current flashed native firmware at every hardware milestone.
12. Commit migration stages separately; do not combine unrelated cleanup with architecture adoption.
13. Do not use M5Paper hardware to discover GUI regressions that can be caught in the simulator.

## Preserved Achievements

The following are acceptance requirements for the adopted tree:

- Real EPUB ZIP/container/OPF/spine/XHTML loading.
- Vietnamese and extended Latin font coverage.
- EPUB chapter pagination and chapter-boundary page turns.
- Recursive library discovery and File Manager opening.
- Home, Library, Favorites, Book Info, Reader, Settings, Connectivity, Sleep, and dialogs.
- Current GUI geometry, visual hierarchy, icons, typography, focus states, and refresh behavior.
- Long-title/author clipping and non-overlapping list rows.
- Rotary short press, long Up/Down press, touch, keyboard, and dock navigation.
- M5Paper IT8951, GT911, power, battery, SD/FatFS, and sleep support.
- PSRAM framebuffer and non-stack placement of large runtime/application objects.
- ZIP parser batched tail lookup and duplicate EPUB-open avoidance.
- Quality/reader-quality/full refresh policy and status-bar DU updates.
- 60-second battery sampling and 10-minute displayed battery cadence.
- `/sdcard` hardware mount behavior.
- Host simulator, Xteink profile, EPUB, font, title, library, architecture, and route tests.

## Target Tree

```text
core/                  ZIP framework, widgets, state, router, capabilities
reader/                Existing EPUB/library/session behavior, adapted to ZIP APIs
app/cpp/               Current GUI and behavior as native C++ application
app/lua/               Lua implementation with native behavior parity
runtime/lua/           Product-neutral Lua bindings and shell standard library
boards/sim/            ZIP Wayland simulator and shared headless simulator runtime
boards/m5paper/       Current hardware runtime adapted to ZIP capability APIs
drivers/               Current IT8951, GT911, and inflate drivers
lang/                  ZIP translation resources
plugins/               ZIP optional feature mechanism
tests/                 Native, Lua, architecture, route, GUI, and hardware-facing tests
```

The current M5Paper implementation is ported into the ZIP tree. The ZIP's simulator and Lua
runtime are adopted; the current hardware code is not replaced by simulator code.

## Migration Phases

### Phase 0: Freeze The Baseline

- Commit all current workspace fixes.
- Record the current commit, firmware binary, test output, and hardware monitor output.
- Capture GUI/framebuffer checkpoints for all important screens.
- Record timing for boot, EPUB open, first page, page turn, and refresh uploads.

Gate: the existing native C++ app remains buildable, testable, and flashable.

### Phase 1: Import ZIP Framework Additions

Adopt without changing product behavior:

- `core/router.hpp`.
- `core/widgets/` components.
- fixed queues and observer improvements.
- Unicode glyph support.
- ZIP structural and architecture tests.

Resolve API conflicts by adapting current callers, not by deleting working capability contracts.

Gate: current native C++ tests and GUI build remain green.

### Phase 2: Port Native C++ Application

- Move current pages into `app/cpp/pages/`.
- Add ZIP `app/cpp/init.hpp`, `main.hpp`, `model.hpp`, and `routes.hpp`.
- Preserve current page geometry and behavior.
- Make route navigation an internal mechanism; do not change visible navigation yet.
- Keep compatibility wrappers only while migration callers are being moved.

Gate: native C++ framebuffer checkpoints and input scenarios match the baseline.

### Phase 3: Adopt Wayland Simulator

- Make ZIP native Wayland the default GUI backend.
- Keep host input routed through the same simulator queue as headless tests.
- Keep any existing backend only as a temporary fallback, not as a second product UI.
- Confirm the Wayland window is only a framebuffer presenter and input adapter.

Gate: native C++ tests, Wayland build, compositor smoke test, and framebuffer parity pass. This
phase must complete before hardware porting is resumed or any new firmware is flashed.

### Phase 4: Port M5Paper Into ZIP Tree

- Port current `boards/m5paper/` and `drivers/` into the ZIP tree.
- Adapt only capability/API boundaries required by the ZIP core.
- Preserve PSRAM framebuffer allocation.
- Preserve `/sdcard` mount point.
- Preserve ESP-IDF entrypoint and board selection.
- Preserve refresh waveform mapping, battery cadence, and rotary long-press handling.

Gate: `idf.py build` passes, then flash/monitor verification passes only after simulator GUI parity
is complete. Hardware is a final validation target, not the primary GUI development loop.

### Phase 5: Add Lua HMI

- Add `runtime/lua/` native bindings and shell modules.
- Add `lua_main.cpp` and `app/lua/` pages/routes/model/init.
- Bind state, library, reader, connectivity, widgets, dialogs, and routing.
- Keep all EPUB and hardware operations native.
- Implement Lua pages to match native C++ output before adding Lua-only style features.

Gate: Lua application tests, parity tests, hot reload tests, and identical GUI checkpoints pass.

### Phase 6: Add Extensions

Only after native/Lua parity:

- translations under `lang/`;
- plugin discovery and generated registries;
- optional Wi-Fi plugin example;
- route-provided dynamic menus;
- transactional Lua hot reload.

Gate: extension tests pass without changing the native baseline behavior.

## Performance Rules

Every optimization currently in the workspace must be carried into the target tree and measured:

- EPUB open latency.
- First content page latency after a cover/index page.
- Page-turn latency.
- ZIP storage read count and byte count.
- IT8951 upload size and duration.
- Refresh mode and dirty rectangle.
- Battery ADC sampling cadence.
- Status-bar update cadence.
- Lua reload duration and state preservation.

A migration is not accepted if a metric regresses without a documented reason and an explicit
decision.

## Hardware Safety Gates

Before flashing any adopted tree:

1. `make test` passes for native C++.
2. Lua, route, architecture, and parity tests pass when enabled.
3. Wayland GUI builds and starts in a compositor session.
4. `idf.py build` passes for the M5Paper target.
5. Current firmware remains available for rollback.

After flashing:

- IT8951 initialization.
- GT911 initialization.
- SD mount at `/sdcard`.
- First framebuffer update.
- EPUB discovery/open/page turn.
- Rotary short and long press.
- Quality/reader-quality/full refresh behavior.
- Battery/status update cadence.
- Sleep/wake.

## Rollback

Rollback means restoring the last known-good native M5Paper firmware, not reverting arbitrary files
from the migration worktree. Each hardware milestone must record:

- source commit;
- firmware image path/hash;
- flash command;
- monitor log;
- manual checks performed.

## Completion Criteria

Adoption is complete only when:

- the ZIP architecture is the active source layout;
- native C++ behavior matches the current GUI and reader baseline;
- Lua pages match native C++ behavior and visual output;
- Wayland is the default host GUI;
- current M5Paper hardware behavior remains available and verified;
- all performance constraints are measured and preserved;
- no current feature is lost or silently simplified;
- the migration status and test reports document any intentional differences.
