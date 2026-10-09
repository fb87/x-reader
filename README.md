# X-Reader UI framework

A small GUI framework, in restricted C++20 (namespaces and structs, no inheritance/virtual
functions/exceptions/RTTI, no dynamic allocation above board capability setup), for building an
ebook reader on e-ink devices. The same app source runs on a 4 bpp M5Paper-style panel and, as a
second simulator-only logical profile, a 1 bpp Xteink-style panel. See `docs/DESIGN.md` for the
full architectural contract and `docs/MIGRATION.md` for current status, including what is and
isn't hardware-verified.

```
make            # build build/reader (the sim-board host executable)
make gui        # build and run the interactive M5Paper Wayland simulator
make test       # build and run the autonomous test suite
```

Run the interactive simulator on a Wayland desktop:

```
./build/simulator_gui
```

It renders the M5Paper 540x960, 4 bpp profile. Use Up/Down for the rotary
controller and Enter for its push button. Down enters the bottom action bar
after the last content control; then Up/Down select actions and Enter runs
the selected action. Up on the first action returns to page content.

With Nix:

```
nix-shell -p gnumake clang wayland pkg-config zlib --run 'make gui'
```

## Layout

```
core/         generic framework API/impl   reader/    EPUB, library, pagination
app/          X-Reader app (pages)         boards/    board-selected capability implementations
drivers/      reusable hardware drivers    fonts/     generated bitmap fonts (shared with ESP-IDF)
tests/        autonomous test suite        tools/     font generator, simulator report
```

`BOARD=sim` (the default) builds the host simulator; `BOARD=m5paper` selects the real ESP-IDF
hardware port (built via `main/CMakeLists.txt` + `idf.py`, not the plain Makefile — see
`docs/MIGRATION.md` for its hardware-verification status).

## Model

| Module | Role |
|---|---|
| `app/cpp` | Native routes, state model, services, and page handlers |
| `app/lua` | Lua pages using the same state and drawing contracts |
| `shell` | Polls input and dispatches events to the active frontend |
| `router` | Maps URIs to pages and maintains navigation history |
| `core/widgets` | Shared status, row, dock, dialog, and progress drawing primitives |
| `canvas`, `text` | Drawing for MONO1, GRAY4, and GRAY8; bitmap fonts; word wrap; ellipsis |
| `display`, `input`, `platform` | The board capability contracts: framebuffer/waveform, events, time/battery/sleep |

### Render pipeline

```
event -> shell callback -> active native/Lua page -> state changes
      -> page render -> framebuffer -> display::update(rect, mode)
```

There is a single framebuffer. Native and Lua pages use the same widget and display primitives.

### Refresh policy

| Mode | Used for |
|---|---|
| `fast` | Moving focus, list selection, clock tick |
| `quality` | Page turns, dialogs, and content changes |
| `full` | Navigation and periodic ghost-clearing refreshes |

`display::device.update_align` snaps partial updates to the controller's alignment, for example 8 px on 1 bpp SPI panels.

## Porting

`boards/sim/runtime.hpp` is a complete working example of the capability surface a board must
publish (display, input, platform, storage; wifi/secret optionally). `boards/m5paper/runtime.hpp`
is a second, real-hardware example (not yet hardware-verified — see `docs/MIGRATION.md`). A new
board needs:

1. a static framebuffer in the panel's format, published via `display::device`
2. `display::device.update(area, mode)` mapped to the controller's waveforms
3. `platform::device` for time/battery, and `enter_deep_sleep` if the board has a low-power state
4. `input::device.poll()` turning buttons/touch into `event::value`s
5. `storage::device` for whatever the board uses for an SD card/filesystem

Select the new board at build time with `BOARD=<name>` (Makefile) or the equivalent CMake/Kconfig
mechanism for an ESP-IDF target; `app/` and `reader/` never reference a board name directly.

## Next steps

- **UTF-8 and Vietnamese**: Alegreya bitmap fonts cover Latin Extended, combining marks, and Vietnamese precomposed letters (U+1EA0–U+1EF9); guarded by `tests/font_coverage_test.cpp`.
- Regenerate `fonts/*.c` under non-`xr_`-prefixed symbol names next time `tools/gen_font.py` is touched (see `docs/MIGRATION.md`'s old-tree-retirement notes).
- 4 bpp glyph bitmaps, to halve font size in flash.
- Spatial focus navigation (up/down/left/right by geometry) for grid layouts.
- Lua application bindings and hot reload (`docs/DESIGN.md` §24/§25) — explicitly deferred.
- Hardware-verify the M5Paper port on real hardware (`docs/MIGRATION.md`'s checklist).
