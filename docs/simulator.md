# PC Simulator

## Purpose

The simulator runs the same core and Lua scripts as firmware. It shortens the
edit-test cycle for UI, EPUB parsing, pagination, rendering, and input behavior
without pretending to replace final M5Paper testing.

## Hot Reload

Development runs may enable script watching with `--watch`. `platform/sdl`
polls the `lua/` directory for changed, added, removed, or atomically replaced
files and debounces editor save sequences. `Ctrl+R` requests a manual reload.
No additional filesystem-watching dependency is required.

Reload replaces the complete Lua VM transactionally:

1. Pause application event delivery.
2. Call the old application's optional `snapshot()` callback.
3. Copy the result into a bounded platform-neutral snapshot.
4. Create a fresh Lua VM and register the restricted bindings.
5. Load all scripts and call `start(snapshot)`.
6. Swap VMs only after successful initialization.
7. Call `stop("reload")` on the old application and destroy its VM.
8. Resume event delivery.

If loading or initialization fails, the old VM continues running. The simulator
reports the error on stderr and in a development overlay, and retries only after
another file change or manual request. In-place `package.loaded` mutation is not
used because stale closures and userdata cannot be rolled back reliably.

Snapshots support nil, booleans, numbers, strings, arrays, and tables with
string keys. Functions, threads, userdata, metatables, and cycles are rejected.
Depth, item count, string length, and total encoded bytes are bounded. Native
handles never cross VM generations. The initial runtime scaffold transfers only
the `screen` string from the returned table; the generic bounded snapshot codec
must be completed before broader application state is persisted across reloads.

Hot reload is not compiled into production firmware. Simulator release runs
leave it disabled unless `--watch` is explicitly supplied.

## Dependencies

Simulator-only dependencies are supplied by the pinned Nix environment:

- SDL2 API for video and input
- zlib for incremental DEFLATE

They are confined to `platform/sdl` and are not linked into firmware.

## SDL And Wayland

The application links SDL2, not Wayland directly. SDL chooses the available
video backend. Normal runs leave `SDL_VIDEODRIVER` unset so Wayland, X11, or
another supported backend can be selected.

Use an explicit variable only for testing:

```bash
SDL_VIDEODRIVER=wayland nix run .#simulator
SDL_VIDEODRIVER=x11 nix run .#simulator
```

The simulator logs the active SDL video driver at startup. `WAYLAND_DISPLAY`,
`XDG_RUNTIME_DIR`, and `DISPLAY` are inherited from the desktop and are not
hard-coded.

## Display

The SDL window presents a fixed 540 x 960 portrait display. The core's packed
4-bit framebuffer is converted to a 32-bit grayscale SDL texture using
nearest-neighbor scaling. The window is not stretched into a landscape aspect
ratio.

The first version models:

- Full and partial refresh modes
- Dirty rectangles
- Configurable refresh delays
- Display busy and completion state
- Visibility of a new frame only after refresh completion

Visual ghosting is not initially simulated.

## Input

- Left mouse press, motion, and release simulate one touch contact.
- Native SDL touch events map to display coordinates.
- Synthesized duplicate touch/mouse events are disabled.
- Arrow keys simulate directional controls.
- Enter simulates the center action.
- A documented shortcut simulates power or sleep.
- `Ctrl+R` requests script reload in watch-enabled development runs.

Keyboard mapping is limited to Up, Down, and Push: arrow Up, arrow Down, and
Enter. Short presses navigate the active content or dock scope; long Up/Down
toggle content and dock focus; long Push toggles Sleep/wake.

Display and input use the same orientation transform.

## Storage

Host paths are explicit so tests do not depend on a user's home directory:

The simulator takes its microSD root from `SDCARD_ROOT`. If unset, it defaults
to `/home/dao/data/sdcard`. Override it explicitly for tests when needed:

```bash
nix run .#simulator -- --library ./books --state ./build/sim-state
```

The equivalent environment configuration is:

```bash
SDCARD_ROOT=/home/dao/data/sdcard nix run .#simulator
```

The library path represents the microSD book directory. The state path stores
simulated persistent settings and reading positions.

## Testing

Most framebuffer conversion, dirty-region, input-transform, and reader tests do
not initialize SDL. The SDL smoke test runs headlessly with:

```bash
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software \
  nix run .#simulator -- --self-test
```

Screenshot assertions compare the CPU-side converted buffer instead of reading
pixels back from an SDL renderer. Native Wayland presentation is a separate
interactive or compositor-backed integration test.
