# Architecture

## Layers

The implementation has three layers and keeps the directory structure aligned
with those layers.

```text
Lua scripts
  application lifecycle, bookshelf, reader, UI, navigation
                    |
Core and Lua binding
  Lua runtime, EPUB, layout, graphics, portable platform contract
                    |
Platform implementation
  ESP-IDF and M5Paper, or SDL2 and host services
```

### Platform Layer

`platform/` contains all target-dependent code.

`platform/esp` provides:

- ESP-IDF application entry point
- M5Paper initialization and power handling
- IT8951 display operations
- GT911 touch input
- FatFs and microSD storage
- ESP-IDF time, settings, and logging integration
- ROM `tinfl` incremental DEFLATE

`platform/sdl` provides:

- Native simulator entry point
- SDL2 display and input
- Host filesystem storage
- Host time and settings
- zlib incremental DEFLATE

This is the only layer that includes ESP-IDF, SDL2, Wayland-related, or zlib
headers. The simulator links SDL2 only and lets SDL choose its Wayland or X11
backend; it does not call Wayland APIs directly.

### Core And Lua Binding Layer

`core/` contains all target-independent C code:

- Lua VM lifecycle and protected callback dispatch
- Packed grayscale framebuffer and graphics
- Bitmap fonts
- ZIP container parsing
- XML and XHTML streaming
- EPUB package, manifest, spine, and navigation processing
- Text layout and pagination
- PNG decoding
- Lua state management and C bindings

`core/include/platform.h` defines the operations required from a platform:

- Display submission and refresh status
- Touch and button events
- Monotonic time and delays
- Book-library storage
- Persistent settings
- Incremental DEFLATE
- Logging
- Shutdown and sleep requests

The contract uses portable C types and a project result type. Public core
headers must not expose `esp_err_t`, FreeRTOS, SDL, Wayland, or zlib types.

The core is a service runtime, not the reader application. It pumps platform
events into Lua and exposes bounded operations without deciding how the reader
responds to those events.

### Lua Script Layer

`lua/` contains the shared application scripts:

- `init.lua` returns the application callback table and selects the initial view.
- `bookshelf.lua` controls book discovery and selection.
- `reader.lua` controls page navigation, menus, and reading interaction.

The application is organized as retained Lua scenes. The scene registry and
stack retain each stage's focus, scroll, selection, and internal mode while a
different scene is active. Loader, Reading, and Sleep are fullscreen; Launcher,
Library, File Manager, and Settings use shared status/content/dock chrome.
See [Lua scenes](scenes.md).

Lua owns application lifecycle policy, reader state, rendering composition,
menus, navigation, and input decisions. It calls only APIs exposed by
`lua_binding.c`. It does not own framebuffer memory, files, decompression
state, parsers, or native document handles.

The application returned by `init.lua` implements this contract:

```lua
return {
    start = function(snapshot) end,
    event = function(event) end,
    update = function(monotonic_ms) end,
    draw = function(canvas) end,
    snapshot = function() return {} end,
    stop = function(reason) end,
}
```

Every callback is invoked with `lua_pcall`. The runtime enforces stack balance,
a bounded allocator, an instruction budget, and consistent error conversion.

## Lua Binding

The binding presents high-level services rather than raw hardware:

- `library` enumerates and identifies books.
- `epub` exposes metadata, navigation, document sessions, and pagination.
- `canvas` draws text, images, lines, rectangles, and clipped content.
- `display` reports dimensions and controls refresh submission and status.
- `settings` reads and writes typed persistent values.
- `system` provides logging, monotonic time, shutdown, and simulator status.

Input is delivered through `event()` rather than exposing a hardware polling
API. Binding functions validate argument types, ranges, and object generation.
Native objects use checked userdata; light userdata and integer pointers are
not exposed. Stale handles fail safely.

The normalized input contract is limited to `up`, `down`, and `push` keys plus
optional touch events. Lua handles debounce, long-press actions, scene focus,
and dock navigation. SDL mouse input is converted to touch coordinates.

Lua has no unrestricted filesystem or native-module access. The runtime omits
unsafe `io`, `os`, and `debug` capabilities and controls script and module
loading. Platform storage operations remain restricted to configured library
and state roots.

## Dependency Direction

The core defines the platform contract, and each target supplies an
implementation when constructing the application. This is dependency
inversion: the core does not depend on a concrete platform even though the
platform implementation includes the core contract.

The entire Lua application depends on core bindings. Neither core nor Lua
scripts contain target-selection conditionals.

## Display Model

The canonical framebuffer is:

- 540 x 960 logical pixels
- Packed 4-bit grayscale
- Two pixels per byte
- High nibble is the left pixel
- 270 bytes per row
- 259,200 bytes total before alignment

Display submission includes a dirty rectangle and either full or partial
refresh mode. A platform may complete asynchronously and reports whether the
display remains busy.

The ESP implementation translates the buffer into IT8951 transfers. The SDL
implementation expands nibbles through a 16-entry grayscale table into an
`XRGB8888` texture.

## Input Model

Both platforms produce one portable event structure for touch and buttons.
Coordinate rotation and orientation are resolved before events reach Lua.

The ESP implementation reads GT911 contacts and physical controls. The SDL
implementation maps mouse and native touch events to contacts and maps keyboard
shortcuts to device buttons.

## Compression Model

ZIP parsing belongs to the core; DEFLATE execution is a platform service.
Firmware uses ESP-IDF ROM `tinfl`, while the simulator uses host zlib. Both
implement the same bounded streaming contract and run against common fixtures.

## Memory Strategy

- Allocate firmware framebuffers in PSRAM where practical.
- Keep SPI DMA buffers in internal DMA-capable RAM.
- Use bounded input and output buffers for ZIP and XML streams.
- Process one spine item at a time.
- Avoid a document DOM and unbounded Lua tables derived from book contents.
- Reuse page and parser workspaces where ownership remains clear.
- Validate sizes before arithmetic and allocation.
- Record free memory and largest allocatable blocks during diagnostics.

The simulator follows firmware limits by default so successful desktop tests do
not accidentally rely on unlimited host memory.

## Concurrency

The initial implementation runs Lua callbacks, parsing, and layout on one
application thread. Platform entry points perform only mechanical event pumping
and callback dispatch. Input may be queued, and display refresh may complete
asynchronously. SDL event handling remains on the simulator main thread.

Additional tasks or threads require a measured need and explicit ownership
rules.

## Error Handling

- Portable APIs return a project result type.
- Platform adapters translate ESP-IDF, SDL, zlib, and host errors at the layer
  boundary.
- Timeouts are mandatory for controller-ready and bus operations.
- Parser errors include entry and input-location context.
- Lua APIs return values or consistent `(nil, error)` results, except for
  programmer errors that raise a Lua error.
- A Lua callback failure is contained by the runtime and never crosses a C
  boundary through `longjmp` without a protected call.
- Unsupported EPUB features are distinct from corrupt input.

## Security Boundaries

EPUB files are untrusted input. Parsers must protect against integer overflow,
truncated records, cyclic references, path traversal, duplicate paths,
excessive nesting, giant attributes, decompression bombs, and unbounded entity
expansion. XML external entity resolution is not permitted.
