# x-reader

`x-reader` is a text-first EPUB reader for e-paper devices, built with ESP-IDF
and Lua. The first hardware target is the original M5Paper. An SDL2 simulator
runs the same core and Lua application on Linux desktops, including Wayland.

The implementation uses C for hardware access and bounded services such as
parsing, layout, and drawing. Lua implements the complete reader application,
including its lifecycle, bookshelf, reader state, UI, and input policy.

## Project Constraints

- Use ESP-IDF and target the original ESP32-based M5Paper first.
- Use `espressif/lua` as the only approved external firmware component.
- Do not use external board, ZIP, XML, EPUB, image, font, or GUI libraries.
- Allow SDL2 and zlib only in the PC simulator platform layer.
- Keep large buffers and native resources under C ownership.
- Process books incrementally instead of loading an entire EPUB into memory.
- Document and reject unsupported EPUB features rather than silently rendering
  them incorrectly.
- Treat formatting, tests, static analysis, sanitizers, fuzzing, reproducibility,
  and explicit resource limits as initial requirements.

Under this policy, "no external libraries" for firmware means ESP-IDF plus the
explicitly approved `espressif/lua` component. Lua is not included in ESP-IDF
itself. Simulator-only dependencies do not become firmware dependencies.

## Architecture

The implementation has three layers:

1. `platform/` contains target-dependent ESP-IDF and SDL2 implementations.
2. `core/` hosts Lua and provides document, layout, drawing, and storage services.
3. `lua/` contains the complete reader application and interaction policy.

The core defines a portable platform contract and contains no ESP-IDF, SDL2,
Wayland, or zlib types. See [Architecture](docs/architecture.md) for details.

## Initial Scope

- Original M5Paper: ESP32, 8 MB PSRAM, 540 x 960 IT8951 e-paper display,
  GT911 touch controller, and microSD storage.
- Reflowable EPUB 2 and EPUB 3 books.
- UTF-8 text with an initially bounded, Latin-oriented bitmap font.
- Paragraphs, headings, lists, emphasis, links, basic alignment, table of
  contents, bookmarks, and reading-position persistence.
- Non-interlaced PNG as the first decoded image format.
- Lua-driven bookshelf and reading interface.

The first deliverable is an architecture skeleton. It establishes layer
interfaces, ownership rules, the build, Lua integration, test fixtures, and a
hardware initialization path. It is not expected to provide a complete reader.

## Deferred Features

- JPEG, GIF, and SVG rendering
- DRM or encrypted EPUB resources
- JavaScript and interactive content
- Embedded fonts and OpenType font parsing
- Fixed-layout EPUB
- MathML, media overlays, audio, and video
- Full CSS layout, floats, positioning, and complex tables
- Bidirectional text and complex-script shaping
- XTEINK hardware support

## Documentation

- [Implementation plan](docs/implementation-plan.md)
- [Architecture](docs/architecture.md)
- [Supported EPUB profile](docs/supported-epub.md)
- [M5Paper hardware target](docs/hardware.md)
- [PC simulator](docs/simulator.md)
- [Development workflow](docs/development.md)
- [Quality and production readiness](docs/quality.md)
- [Lua scene architecture](docs/scenes.md)
- [Lua widget architecture](docs/widgets.md)
- [Parser progress](docs/parser.md)

## Toolchain Status

The reproducible build uses Nix with pinned ESP-IDF 5.5.2 tooling. At the time
this plan was written, `idf.py` was not available outside a Nix environment.
