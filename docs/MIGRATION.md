# Migration Status

Tracks progress of `docs/DESIGN.md`'s refactor, on branch `refactor/cpp-design` (branched from
`dev/minimal`). `dev/minimal` remains the behavioral reference throughout — nothing here is
claimed as done until it has test parity against that branch.

This document favors honesty over completeness: a step is listed as done only once a Make target
actually exercises it, and the hardware phase is never described as "verification remaining" for
code that does not exist or has not been built/flashed.

## Done, simulator-verified

- `core/`: geometry, refresh (incl. the dirty-rect scheduler), event, display, platform, input,
  storage, capability, connectivity, secret, state, canvas, text, widget, page, dialog, shell.
  Verified with differential tests against the real old C implementation using real production
  font data and an 18-step scripted shell scenario (navigation, dialogs, refresh merging/
  promotion) — byte-identical output.
- `reader/`: epub (real bounded ZIP/OPF/XHTML parser), book (Vietnamese mojibake title repair),
  library (BFS scan over `storage::list`), session (EPUB-backed pagination/chapter-turn/
  progress). Verified against the real 2.8MB sample.epub fixture (602 manifest / 596 spine items,
  real Vietnamese chapter text) and the real book-title mojibake fixtures — byte-identical to the
  old implementation.
- `boards/sim/`: the M5Paper logical profile (540x960 GRAY4) over host storage/zlib, plus the
  interactive SDL2 GUI (`boards/sim/gui.cpp`), confirmed rendering correctly on a real Wayland
  session. A real `secret::store` backs `capability::id::secret` here (fixed in-memory table,
  never persisted to disk).
- `app/`: all eight pages/dialogs (splash, home, library, favorites, file manager, settings,
  sleep, reader, book-info) wired to real `reader::session`/`reader::library`, real fonts (full
  Vietnamese coverage, bridged from the same compiled `fonts/*.c` objects the old app uses — not
  copies), and `state::store`-backed, checkpointed settings.
- `tests/`: `simulator_test.cpp` (47 input-injection checks covering splash timing, library
  discovery, the full EPUB-open pipeline, touch/chrome, back navigation, favorites, settings
  mutation, delete confirmation, sleep/wake, and a persistence round trip — exclusively through
  `board::sim::inject/touch -> app::pump -> shell::dispatch`, never by calling a page handler
  directly), `epub_test.cpp` (real-fixture EPUB regression), `font_coverage_test.cpp` (guards the
  real Vietnamese/combining-mark glyph coverage).
- `make test` passes end to end; the untouched `dev/minimal`-era Makefile targets (`make run`,
  `make test-title`, `make test-epub`) still pass unmodified.

Simplifications made along the way, flagged rather than silently dropped:
- File Manager opens a book through the same 16-book-capped `reader::library::add` used by
  scanning, not the old app's dedicated 17th "transient" slot (`app_open_storage_epub`'s overflow
  path in the old `app/app.c`).
- No app-level credential-entry flow exists yet to exercise the real secret store with (the
  ported Settings page's Wi-Fi/Bluetooth rows are UI-only booleans, matching the old C app
  exactly — it never had a real connect-with-password UI either).

## Done, but NOT hardware-verified — no ESP-IDF toolchain in this sandbox

- `drivers/it8951/`, `drivers/gt911/`, `drivers/inflate/`: ported from `port/m5paper/drivers/*`
  and `port/m5paper/inflate.*`, preserving every piece of hardware-verified logic verbatim —
  the 90-degree rotation coordinate remap, the chunked/DMA-safe pixel upload with periodic
  watchdog resets, GC16/DU waveform selection, and the dual I2C address probe (0x14/0x5d).
- `boards/m5paper/`: `pins.hpp`/`board.hpp`/`.cpp` ported verbatim (power rails, battery ADC
  curve, deep sleep). `runtime.hpp` is new: it composes the drivers above plus ESP-IDF's SD/FatFS
  VFS into the exact same capability surface `boards/sim/runtime.hpp` publishes.
- `enter_deep_sleep()` — present in the old `board.cpp` but never called from the old
  `app_main.cpp`'s loop — is now wired through `platform::device.enter_deep_sleep`, closing that
  gap for real on the one board where it matters.
- `main/CMakeLists.txt` points at the new source tree (`app_main.cpp`, the three driver `.cpp`
  files, `boards/m5paper/board.cpp`, `fonts/*.c`) and requests `cxx_std_20` with
  `-fno-exceptions -fno-rtti`. `sdkconfig` already disables C++ exceptions/RTTI at the Kconfig
  level (`CONFIG_COMPILER_CXX_EXCEPTIONS`/`CONFIG_COMPILER_CXX_RTTI` are both unset), so no
  Kconfig change was needed there. `partitions.csv`'s 6MB factory partition has ample headroom
  for the new tree's comparable binary size — reviewed, not changed.

**None of the above has been compiled, flashed, or run.** This sandbox has no ESP-IDF toolchain
(the `flake.nix` that would fetch one was never exercised in this session). The deliverable is
code and build wiring believed correct via careful line-by-line preservation of the old driver
logic, not a verified result. Before this phase can be called done, the user must, on real
M5Paper hardware:

1. `idf.py build` — confirms the pinned ESP-IDF toolchain actually accepts C++20 for the xtensa
   target (not independently confirmed here).
2. `idf.py flash monitor`.
3. Manually verify: rotation correctness (no mirrored/offset partial updates), page-turn latency
   (no watchdog reset under the chunked upload), touch responsiveness at both I2C addresses,
   battery percentage sanity against a multimeter, and sleep/wake via both the rotary-press ext0
   wakeup and the timer fallback.

## Not started

Xteink (simulator-only logical profile; no real hardware driver exists anywhere today, old or
new tree). Old-tree retirement (`include/xr/`, `src/`, `port/sim/`, `port/sdl/`, `port/m5paper/`
— the last of these stays until `boards/m5paper/` has had its real-hardware pass).

## Explicitly out of scope for this pass

- Lua application bindings and hot reload (`docs/DESIGN.md` §24/§25).
