# X-Reader UI framework

A small GUI framework, in restricted C++20 (namespaces and structs, no inheritance/virtual
functions/exceptions/RTTI, no dynamic allocation above board capability setup), for building an
ebook reader on e-ink devices. The same app source runs on a 4 bpp M5Paper-style panel and, as a
second simulator-only logical profile, a 1 bpp Xteink-style panel. See `docs/DESIGN.md` for the
full architectural contract and `docs/MIGRATION.md` for current status, including what is and
isn't hardware-verified.

```
make            # build build/reader (the sim-board host executable)
make gui        # build and run the interactive M5Paper SDL2 simulator
make test       # build and run the autonomous test suite
make fonts      # regenerate bitmap fonts (needs Python + Pillow; Alegreya needs ALEGREYA_FONT_DIR)
```

Run the interactive simulator on a desktop with SDL2 installed:

```
./build/simulator_gui
```

It renders the M5Paper 540x960, 4 bpp profile. Use Up/Down for the rotary
controller and Enter for its push button. Down enters the bottom action bar
after the last content control; then Up/Down select actions and Enter runs
the selected action. Up on the first action returns to page content.

With Nix:

```
nix-shell -p gnumake clang SDL2 pkg-config zlib --run 'make gui'
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

**Regions** (where things go) and **layers** (what draws on top) are kept separate:

```
┌──────────────────────┐  status bar  ─┐
│                      │                ├─ layer 1: content (chrome + top page)
│      page area       │                │
│   ┌──────────────┐   │  ← layer 2: dialogs (sized to content, modal)
│   └──────────────┘   │                │
├──────────────────────┤  dock         ─┘
└──────────────────────┘  layer 0: background (wallpaper / sleep image)
```

Each page declares the chrome it wants (`page::chrome::status | page::chrome::dock`). The shell
gives the page an `area` that covers whatever space the chrome leaves free. The Reader page runs
without chrome and toggles it on a tap. When chrome is toggled, the page gets `on_layout` and
repaginates.

| Module | Role |
|---|---|
| `shell` | Owns the screen: page stack (`push/pop/replace`), dialog stack, input routing, status bar, dock, composition |
| `page` | Mechanism for a screen. Lifecycle: `on_create / on_layout / on_enter / on_exit / on_destroy`, plus `render`, `on_event`, `on_action`, `on_tick` |
| `dialog` | Mechanism for modal popups. `layout()` sizes the dialog from its content. The result is delivered through a callback. Ships with `dialog::confirm` (Yes/No) |
| `widget` | Base widget, intrusive tree, focus scope. Stock widgets: label, button, list |
| `refresh` | Dirty-rect scheduler: merges rects and promotes to a full flash after N quality updates |
| `canvas`, `text` | Drawing for MONO1, GRAY4, and GRAY8; bitmap fonts; word wrap; ellipsis |
| `display`, `input`, `platform` | The board capability contracts: framebuffer/waveform, events, time/battery/sleep |

### Subclassing pattern

```cpp
struct library_page { page::context base{}; widget::list list{}; /* base first */ };

inline void library_create(page::context& p) {
  library_page& lp = library_of(p);  // offsetof-based recovery, see app/app.hpp
  ...
}
inline constexpr page::vtbl library_vtbl{library_create, /* ... */};
```

### Render pipeline

```
event -> shell routes it (top dialog, otherwise page -> focused widget -> dock shortcut -> BACK pops)
      -> state changes -> shell::invalidate(rect, fast | quality | full)
      -> shell::flush(): for each merged dirty rect
            compose layers 0..2 into the framebuffer, clipped to the rect
            display::update(rect, mode)
```

There is a single framebuffer. Closing a dialog invalidates its rect, and the layers underneath are redrawn there, so no save-under buffer is needed.

### Refresh policy

| Mode | Used for |
|---|---|
| `fast` | Moving focus, list selection, clock tick |
| `quality` | Page turns, dialogs, content changes; counted toward the ghosting budget |
| `full` | Navigating to a new page, or once the budget is spent (`shell::set_full_refresh_every`) |

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
