# Architecture

## Layering

The firmware is divided into layers with one-way dependencies:

```text
app
  ui
    layout
      epub
  gfx
  storage
  board interface
    m5paper backend
```

The EPUB and layout layers must be testable without hardware. UI code may use
the board interface and graphics layer, but drivers must not know about EPUB
or application state.

## Modules

### `app`

Owns startup, task coordination, state transitions, and fatal-error policy.
`app_main()` is the only required C-linkage entry point.

### `board`

Defines board-neutral display, input, storage, clock, and power operations.
Each board backend owns its GPIO numbers, bus configuration, controller
initialization, and hardware timing.

### `drivers`

Contains handwritten peripheral drivers:

- IT8951E over SPI
- GT911 over I2C
- BM8563 over I2C
- SD card over SPI/SDMMC as supported by the board
- Rotary switch and physical controls over GPIO
- Board power and sleep controls

Drivers expose explicit state structs and init/deinit or start/stop functions.

### `gfx`

Owns framebuffer memory, drawing primitives, dirty regions, glyph rendering,
UTF-8 decoding, and display refresh requests. It does not parse EPUB content.

### `epub`

Owns file-backed ZIP access, CRC32, DEFLATE, XML tokenization, package
metadata, spine order, XHTML extraction, and limited CSS. It produces bounded
document data for the layout layer instead of drawing directly.

### `layout`

Converts document nodes and style data into lines, blocks, and pages for the
current display geometry and font settings. Pagination must be deterministic
for the same book, settings, and viewport.

### `ui`

Owns screen state and user interaction. It renders through `gfx`, requests
content from `epub` and `layout`, and stores user preferences through
`storage`.

### `storage`

Owns SD-card paths, reading positions, bookmarks, settings, and NVS data. It
must distinguish removable book data from device-local state.

## Memory Rules

- Prefer caller-owned buffers and bounded structs.
- Allocate large framebuffers and decompression buffers from PSRAM when safe.
- Never retain a pointer to a temporary parser buffer.
- Every allocation has one visible owner and one release path.
- Parser input must support streaming where possible.
- Allocation failure is a normal error path, not an assertion-only condition.

## Board Portability

Board-specific code is selected at build time. Shared code may depend only on
the board interface. A future Xteink 4 backend must provide the same logical
operations while supplying different dimensions, pins, controller commands,
refresh behavior, and sleep behavior.
