# Implementation Plan

## Goal

Create one portable reader core with two platform implementations: ESP-IDF for
the original M5Paper and SDL2 for a PC simulator. Both targets run the same Lua
application and incremental EPUB processing pipeline. C supplies bounded
services; Lua owns all reader behavior and UI policy.

## Minimal Repository Layout

```text
x-reader/
|-- .gitignore
|-- .clang-format
|-- .clang-tidy
|-- .stylua.toml
|-- .githooks/
|   `-- pre-commit
|-- CMakeLists.txt
|-- flake.nix
|-- flake.lock
|-- sdkconfig.defaults
|-- platform/
|   |-- esp/
|   |   |-- CMakeLists.txt
|   |   |-- main.c
|   |   |-- platform.c
|   |   |-- display_it8951.c
|   |   |-- input_gt911.c
|   |   |-- storage.c
|   |   `-- inflate.c
|   `-- sdl/
|       |-- CMakeLists.txt
|       |-- main.c
|       |-- platform.c
|       |-- display.c
|       |-- input.c
|       |-- storage.c
|       `-- inflate.c
|-- core/
|   |-- CMakeLists.txt
|   |-- include/
|   |   |-- platform.h
|   |   |-- runtime.h
|   |   |-- display.h
|   |   |-- input.h
|   |   `-- epub.h
|   `-- src/
|       |-- runtime.c
|       |-- graphics.c
|       |-- font.c
|       |-- zip.c
|       |-- xml.c
|       |-- epub.c
|       |-- layout.c
|       |-- png.c
|       `-- lua_binding.c
|-- lua/
|   |-- init.lua
|   |-- app.lua
|   |-- chrome.lua
|   |-- input.lua
|   `-- scenes/
|       |-- loader.lua
|       |-- launcher.lua
|       |-- library.lua
|       |-- file_manager.lua
|       |-- settings.lua
|       |-- reading.lua
|       `-- sleep.lua
|-- tools/
|   `-- format
|-- tests/
|-- docs/
`-- worktree/
```

`worktree/` is ignored and exists only when linked Git worktrees are present.
It is not part of the source tree.

## Phase 1: Repository And Toolchain

1. Add `/worktree/`, `/build/`, and Nix result links to `.gitignore`.
2. Initialize Git on `main` and create a baseline documentation commit.
3. Create `worktree/simulator-foundation` on a dedicated feature branch.
4. Pin nixpkgs and `nixpkgs-esp-dev` in `flake.lock`.
5. Use the overlay's ESP-IDF 5.5.2 Xtensa toolchain.
6. Fetch one exact Espressif Lua source revision with a Nix hash and use it for
   both targets.
7. Add firmware, simulator, and combined development shells.
8. Add firmware and simulator package outputs and checks.
9. Enable `CONFIG_APP_REPRODUCIBLE_BUILD` and configure PSRAM, FatFs, and SD.
10. Pin clang-format, clang-tidy, and StyLua as build-time tools.
11. Add tracked formatting configuration and a safe pre-commit hook.

## Phase 2: Three-Layer Skeleton

Create the `platform`, `core`, and `lua` layers. Incomplete behavior may return
an explicit unsupported result, but every interface must define:

- Resource ownership and destruction
- Error and timeout behavior
- Thread-safety expectations
- Allocation limits
- Stream read, seek, and close behavior
- Parser callback lifetimes
- Display coordinates and pixel format
- Portable input events
- Versioned Lua APIs
- Lua allocator and callback instruction budgets

The core defines a portable result type and platform contract. It must not
expose `esp_err_t`, SDL, FreeRTOS, Wayland, or zlib types.

## Phase 3: Platform Lifecycle

The ESP entry point initializes:

1. Logging and settings
2. PSRAM
3. GPIO, SPI, and I2C
4. E-paper display
5. Touch and physical input
6. microSD and FatFs
7. Core reader services
8. Lua and application scripts
9. The mechanical event pump and protected Lua callback loop

The SDL entry point initializes host storage, a 540 x 960 logical display,
input, zlib-backed inflation, core services, Lua, and the same event loop.

Every stage reports a precise error. Missing storage, input, or display services
must not result in an indefinite wait.

## Phase 4: Lua Integration

- Compile the same pinned Lua source for both targets.
- Embed initial scripts in firmware and make them directly available to the
  simulator build.
- Require `init.lua` to return `start`, `event`, `update`, `draw`, `snapshot`,
  and `stop` callbacks.
- Expose high-level `library`, `epub`, `canvas`, `display`, `settings`, and
  `system` modules.
- Represent native resources as checked userdata.
- Keep framebuffers, files, decompression state, parsers, and pages owned by C.
- Exclude dynamic native module loading and unsafe operating-system functions.
- Return consistent errors to scripts.
- Invoke all callbacks through protected calls with stack-balance checks.
- Enforce memory and instruction limits for application callbacks.

Lua controls the complete application lifecycle, bookshelf behavior, reader
state, menus, navigation, rendering composition, and input policy. It delegates
pixel loops, archive decompression, XML tokenization, pagination, and native
resource ownership to C services.

The application uses retained scene stages and a unique stack. Loader replaces
Launcher; Launcher pushes Library, File Manager, and Settings; Library and File
Manager push Reading; Sleep is pushed over the current scene and pops on wake.
Back reveals the previous stage without resetting its selection, scroll, focus,
or dock highlight. See `docs/scenes.md`.

## Phase 5: Processing Pipeline

```text
Platform storage
  -> ZIP entry stream
  -> XML/XHTML events
  -> EPUB document events
  -> text and image layout
  -> packed grayscale framebuffer
  -> portable display submission
  -> IT8951 or SDL2 update
```

Every boundary uses bounded buffers. No interface requires loading a whole
book or an unbounded chapter into memory.

## Phase 6: Simulator Behavior

- Expand the packed 4-bit framebuffer into an SDL `XRGB8888` texture.
- Preserve the 540 x 960 logical size in resizable windows.
- Map mouse, touch, and keyboard input into portable events.
- Model full and partial refresh timing, dirty regions, and busy state.
- Use explicit library and state paths.
- Log the selected SDL video backend.
- Support SDL's dummy video driver for headless smoke tests.
- Do not simulate visual ghosting initially.
- Start the simulator with a fixed 540 x 960 portrait window; preserve logical
  aspect ratio rather than stretching the framebuffer.
- Add `--watch` and `Ctrl+R` development reload controls.
- Poll and debounce changes under `lua/` without another dependency.
- Replace Lua VMs transactionally and preserve a bounded data-only snapshot.
- Keep the old application running when new scripts fail to initialize.
- Reject functions, userdata, threads, metatables, cycles, and excessive data
  in reload snapshots.

## Phase 7: Formatting And Analysis

- Use clang-format with LLVM style and 4-space indentation for project C code.
- Use StyLua for scripts.
- Run the same `tools/format` implementation from `nix fmt`, pre-commit, and
  Nix checks.
- Safely autoformat and re-stage fully staged files.
- Reject partially staged files when their staged content needs formatting.
- Compile project C as C11 with strict warnings treated as errors.
- Run clang-tidy and a host static analyzer on core and simulator code.
- Validate Lua syntax and architecture boundaries.

## Phase 8: Verification

- Build firmware with `nix build .#firmware`.
- Build the simulator with `nix build .#simulator`.
- Run all checks with `nix flake check --no-update-lock-file`.
- Test core code without initializing SDL.
- Run SDL smoke tests with dummy video and software rendering.
- Test native Wayland presentation when a compositor is available.
- Check in small valid and malformed ZIP, XML, and EPUB fixtures.
- Run the same compression fixtures against zlib and ROM `tinfl` backends.
- Log firmware memory and stack high-water marks.
- Perform hardware smoke tests for PSRAM, SD, display, and touch.
- Run host tests under ASan and UBSan.
- Replay bounded fuzz corpora for all untrusted parsers and Lua bindings.
- Compare two clean firmware builds for reproducibility.
- Check dependency licenses, simulator closure, and firmware link map.
- Test retained scene stacks, Back restoration, dock focus, three-key input,
  long Push sleep/wake, optional mouse touch activation, and fullscreen layout.

## Skeleton Acceptance Criteria

- Nix builds are pinned and require no network after inputs are fetched.
- Firmware builds with ESP-IDF 5.5.2 and only the approved Lua component.
- Simulator dependencies remain limited to its platform layer.
- Firmware and simulator execute the same core and Lua scripts.
- Lua owns application and reader policy; C exposes services only.
- Lua callbacks are protected and resource-bounded.
- Simulator reload is transactional and preserves the old application on error.
- Shared headers contain no ESP-IDF, SDL, Wayland, or zlib types.
- The SDL simulator works through Wayland and permits X11 fallback.
- Headless SDL smoke tests work with the dummy video driver.
- Input reaches Lua through the same portable event API on both targets.
- Full and partial refreshes honor dirty regions and configurable timing.
- A minimal EPUB fixture passes through ZIP, XML, EPUB, and layout interfaces.
- `nix fmt` applies clang-format and StyLua, and format checks reject drift.
- Static analysis, sanitizers, fuzz smoke tests, and Lua syntax checks pass.
- Parser, memory, timeout, persistence, and artifact-size limits are explicit.
- No external board, ZIP, XML, EPUB, PNG, font, or GUI library is imported into
  firmware.

## Later Reader Milestones

1. Complete safe ZIP indexing and streaming decompression.
2. Complete streaming XML parsing and EPUB package discovery.
3. Implement OPF manifest, spine, navigation, and bookshelf metadata.
4. Add text normalization, layout, pagination, bitmap fonts, and positions.
5. Implement non-interlaced PNG, grayscale conversion, scaling, and dithering.
6. Add measured e-paper partial-refresh and ghosting-management policy.
7. Optimize chapter replay, backward navigation, caching, sleep, and battery
   usage.
8. Add another device as a new platform implementation only after identifying
   its exact hardware revision.

## Non-Goals For The Skeleton

- Complete EPUB rendering
- Broad CSS compatibility
- JPEG decoding
- Simulator ghosting effects
- Final measured power and sleep policy
- XTEINK support
- A plugin compatibility promise
