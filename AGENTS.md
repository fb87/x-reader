# Repository Instructions

## Current State

- The repository is documentation-only until root manifests and source directories exist; do not report planned commands as verified.
- Read `README.md`, `docs/architecture.md`, `docs/development.md`, and `docs/quality.md` before changing architecture or tooling.
- Read `docs/hardware.md` or `docs/supported-epub.md` before changing the corresponding platform or parser behavior.
- Once executable config exists, trust `flake.nix`, CMake files, hooks, and checks over prose; update stale docs in the same change.

## Architecture Contract

- Keep exactly three implementation layers: target code in `platform/`, portable C services and Lua bindings in `core/`, and the complete reader application in `lua/`.
- Lua owns lifecycle policy, bookshelf, reader state, navigation, UI, input decisions, and rendering composition. C is a bounded service runtime, not the reader application.
- `core/include/platform.h` is the portability boundary. Public core headers must not expose ESP-IDF, FreeRTOS, SDL, Wayland, or zlib types.
- Keep native files, parsers, framebuffers, and decompression state in checked C userdata; never expose pointers, integer handles, or light userdata to Lua.
- Bind high-level `library`, `epub`, `canvas`, `display`, `settings`, and `system` services. Input is delivered through the Lua application callback, not polled as raw hardware.
- Keep Lua scenes retained behind a unique stack: Back pops to the previous stage and must restore its highlight, scroll, focus, and dock selection.
- Loader, Reading, and Sleep are fullscreen; Launcher, Library, File Manager, and Settings use shared status/content/dock chrome.
- The physical input contract is Up, Down, and Push; SDL mouse is optional touch. Long Up/Down toggle content and dock focus; long Push toggles Sleep/wake.
- Do not add a blanket `xr` prefix. Use descriptive module names such as `epub_open`, `display_present`, and `layout_next_page`.
- Preserve the minimal layout in `docs/implementation-plan.md`; split modules only for a demonstrated ownership or build need.

## Platform Constraints

- Firmware targets the original ESP32-based M5Paper first: IT8951E display, GT911 touch, microSD, and 8 MB PSRAM.
- Firmware may use ESP-IDF plus the pinned Espressif Lua component only. Do not add board, ZIP, XML, EPUB, image, font, or GUI libraries.
- SDL2 and zlib are simulator-only and must remain under `platform/sdl`; link SDL2 rather than Wayland directly.
- The canonical framebuffer is packed 4-bit grayscale at 540 x 960, high nibble first, 270 bytes per row.
- ZIP parsing is shared core code; firmware DEFLATE uses ROM `tinfl`, while the simulator implements the same bounded contract with zlib.

## Lua Runtime

- Invoke `start`, `event`, `update`, `draw`, `snapshot`, and `stop` only through protected Lua calls with stack, memory, and instruction limits.
- Keep unsafe `io`, `os`, `debug`, unrestricted script loading, and native module loading unavailable.
- Simulator hot reload replaces the whole VM transactionally; never reload by mutating `package.loaded` in place.
- On reload failure, keep the old VM running. Snapshots are bounded, acyclic data only and must reject functions, threads, userdata, and metatables.
- Hot reload is simulator-only; `--watch` enables file watching and `Ctrl+R` requests a manual reload.
- After every Lua edit, run `nix fmt`, allow the watched simulator to reload, and verify the existing simulator process remains alive before making the next change.

## Build And Verification

- The intended pinned toolchain is Nix, ESP-IDF 5.5.2, C11, clang-format, clang-tidy, and StyLua.
- After `flake.nix` exists, use `nix build .#firmware`, `nix build .#simulator`, and `nix flake check --no-update-lock-file`.
- Run the simulator with `nix run .#simulator -- --library ./books`; force Wayland only for testing with `SDL_VIDEODRIVER=wayland`.
- Run headless SDL checks with `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`.
- No focused-test command is defined yet. Add and document one when the test harness is created rather than inventing an ad hoc command.
- Do not run ESP-IDF installers, `pip install`, or network-enabled component resolution inside builds.

## Formatting

- Do not introduce C or Lua before the same change establishes the pinned formatter configuration and checks.
- Format every C, header, and Lua change before build or test; once available, the authoritative command is `nix fmt`.
- C uses `.clang-format` based on LLVM with 4-space indentation; Lua uses pinned StyLua and `.stylua.toml`.
- The tracked pre-commit hook autoformats and re-stages fully staged files, but must reject rather than mutate partially staged files needing formatting.
- Formatting checks in `nix flake check` are authoritative because hooks can be bypassed.

## Production Baseline

- Apply warnings as errors, static analysis, ASan/UBSan host tests, bounded fuzz smoke tests, reproducibility checks, and dependency/license checks from the first skeleton.
- Treat EPUBs as untrusted input. Centralize limits and checked arithmetic for archive expansion, paths, XML depth/tokens, PNG dimensions, allocations, and parser work.
- Require timeouts for hardware, storage, parser, event, and Lua callback operations; no operation may wait indefinitely.
- Persistence must use versioned, checksummed, power-loss-safe records; test interrupted writes in the simulator.
- A simulator-successful artifact is not production firmware. Hardware release gates are defined in `docs/quality.md`.

## Git Worktrees

- Initialize the repository on `main`, commit the documentation baseline, then perform implementation in a linked topic worktree.
- Keep all linked worktrees under ignored `/worktree/`; also exclude that directory from Nix and recursive tooling.
- Configure tracked hooks with `git config --local core.hooksPath .githooks`; the relative path must work from linked worktrees.
- Do not use `git clean -fdx` from the primary checkout because it ignores the `/worktree/` protection.
