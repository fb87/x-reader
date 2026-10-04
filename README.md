# X-Reader UI framework

A small GUI framework, written in C99, for building an ebook reader on e-ink devices. It is OOP in C (structs and vtables), uses no `malloc`, and has no hardware code above the HAL. The same app source runs on a 4 bpp M5Paper-style panel and on a 1 bpp Xteink-style panel.

```
make run        # build the simulator, run a scripted session, write out/*_sheet.png
make gui        # build the interactive M5Paper SDL2 simulator
make fonts      # regenerate bitmap fonts (needs Python + Pillow; Alegreya needs ALEGREYA_FONT_DIR)
```

Run the interactive simulator on a desktop with SDL2 installed:

```
./build/xr_m5paper_gui
```

It renders the M5Paper 540x960, 4 bpp profile. Use Up/Down for the rotary
controller and Enter for its push button. Down enters the bottom action bar
after the last content control; then Up/Down select actions and Enter runs
the selected action. Up on the first action returns to page content.
Hold Down for 500 ms to jump directly to the first action in the bottom bar.
Hold Up for 500 ms while in the bar to return to page content.

With Nix:

```
nix-shell -p gnumake SDL2 pkg-config --run 'make gui && ./build/xr_m5paper_gui'
```

## Layout

```
include/xr/   framework API          src/        framework implementation
app/          X-Reader app (pages)   fonts/      generated bitmap fonts
port/sim/     host simulator port    port/template/  starting point for a board
tools/        font generator, simulator report
```

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

Each page declares the chrome it wants (`XR_CHROME_STATUS | XR_CHROME_DOCK`). The shell gives the page an `area` that covers whatever space the chrome leaves free. The Reader page runs without chrome and toggles it on a tap. When chrome is toggled, the page gets `on_layout` and repaginates.

| Module | Role |
|---|---|
| `xr_shell` | Owns the screen: page stack (`push/pop/replace`), dialog stack, input routing, status bar, dock, composition |
| `xr_page` | Base class for a screen. Lifecycle: `on_create / on_layout / on_enter / on_exit / on_destroy`, plus `render`, `on_event`, `on_action`, `on_tick` |
| `xr_dialog` | Base class for modal popups. `layout()` sizes the dialog from its content. The result is delivered through a callback. Ships with `xr_confirm` (Yes/No) |
| `xr_widget` | Base widget, intrusive tree, focus scope. Stock widgets: label, button, list |
| `xr_refresh` | Dirty-rect scheduler: merges rects and promotes to a full flash after N quality updates |
| `xr_canvas`, `xr_text` | Drawing for MONO1, GRAY4, and GRAY8; bitmap fonts; word wrap; ellipsis |
| `xr_hal.h` | The full porting surface: display, platform, and event types |

### Subclassing pattern

```c
typedef struct { xr_page_t base; xr_list_t list; } library_page_t;  /* base first */

static void library_create(xr_page_t *p) {
    library_page_t *lp = XR_CONTAINER_OF(p, library_page_t, base);
    ...
}
static const xr_page_vtbl_t k_vtbl = { .on_create = library_create, .on_action = ... };
```

### Render pipeline

```
event → shell routes it (top dialog, otherwise page → focused widget → dock shortcut → BACK pops)
      → state changes → xr_shell_invalidate(rect, FAST | QUALITY | FULL)
      → xr_shell_flush(): for each merged dirty rect
            compose layers 0..2 into the framebuffer, clipped to the rect
            display->update(rect, mode)
```

There is a single framebuffer. Closing a dialog invalidates its rect, and the layers underneath are redrawn there, so no save-under buffer is needed.

### Refresh policy

| Mode | Used for |
|---|---|
| `FAST` | Moving focus, list selection, clock tick |
| `QUALITY` | Page turns, dialogs, content changes; counted toward the ghosting budget |
| `FULL` | Navigating to a new page, or once the budget is spent (`xr_shell_set_full_refresh_every`) |

`display->update_align` snaps partial updates to the controller's alignment, for example 8 px on 1 bpp SPI panels.

## Porting

Copy `port/template/xr_port_template.c` and provide:

1. a static framebuffer in the panel's format
2. `update(rect, mode)` mapped to the controller's waveforms
3. `now_ms`, wall time, and battery level
4. a loop that polls input, then calls `dispatch → tick → flush`

`port/sim` is a complete working example.

## Next steps

- **UTF-8 and Vietnamese**: Alegreya bitmap fonts cover Latin Extended, combining marks, and Vietnamese precomposed letters (U+1EA0–U+1EF9).
- 4 bpp glyph bitmaps, to halve font size in flash.
- Spatial focus navigation (up/down/left/right by geometry) for grid layouts.
- Rotation in the display port.
- A EPUB/TXT layout engine to replace the reader's sample text.
