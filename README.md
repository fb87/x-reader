# XReader

XReader is a native ESP-IDF EPUB reader for the M5Paper K049. The firmware is
written in a deliberately small C++ subset: C-style code with namespaces,
snake_case naming, explicit ownership, and no C++ runtime features beyond
what the ESP-IDF toolchain requires.

The first hardware target is the M5Paper K049. The software is structured so
that a future Xteink 4 board backend can reuse the EPUB, layout, storage, and
UI layers.

## Project Status

The repository currently contains the project baseline and design documents.
Hardware drivers and the EPUB implementation have not been added yet.

## Constraints

- Native ESP-IDF only. No Arduino, PlatformIO, M5Stack libraries, LVGL, or
  managed third-party components.
- All display, input, storage, graphics, EPUB, XML, ZIP, and DEFLATE code is
  handwritten in this repository.
- C++ is used only for namespaces and normal C-compatible data structures and
  functions.
- Nix provides the host development environment and pins its inputs.

## Documents

- [Project scope](docs/project-scope.md)
- [Architecture](docs/architecture.md)
- [GUI design](docs/gui-design.md)
- [Hardware](docs/hardware.md)
- [Coding rules](docs/coding-rules.md)
- [Development workflow](docs/development.md)
- [Fonts](docs/fonts.md)
- [Milestones](docs/milestones.md)

## Initial Commands

Enter the pinned development shell:

```sh
nix develop
```

After the ESP-IDF toolchain is available in the shell:

```sh
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

The exact ESP-IDF setup is intentionally documented in
`docs/development.md`; the `nixos-26.05` package set does not currently
provide a complete native ESP-IDF toolchain package.

## Visual redesign update

The UI layer now contains a visible e-paper-oriented redesign rather than only structural framework changes:
- card-based Home screen
- book-card Library rows with focus treatment
- two-column Settings rows
- centered Reader Menu / Quick Settings panel
- light status chrome and segmented bottom action bar
- responsive geometry retained for 800x480 and 960x540 targets

## Orientation update

Reader orientation is now a persisted setting. The Reader Menu / Quick Settings and the full Settings screen expose `ORIENTATION` with `LANDSCAPE` and `PORTRAIT` values. Switching orientation reconfigures the IT8951 rotation, recreates the framebuffer with swapped logical dimensions, repaginates the current chapter, and remaps touch coordinates into the logical viewport. Physical button navigation is orientation-independent.

## XTeink X4 status

The generic UI/action framework supports button-only navigation, and an XTeink X4 ADC button
backend is included under `main/board/xteink/`. The application still defaults to the M5Paper
IT8951E display path; SSD1677 + shared-SPI display/storage integration is the next X4 bring-up
step. See `docs/xteink-x4.md`.

## Extended screens (2026-09-29)

The UI framework now includes the first post-framework screen expansion:

- Reader Menu entries for Contents, Bookmarks, Book Info, and Add Bookmark
- Table of Contents screen using EPUB NCX/nav metadata with spine fallback
- Bookmarks screen with physical-button and touch navigation
- Book Info screen showing title, author, chapter count, and current chapter
- Reader Menu routes directly into these screens and Back returns to reading
- Table-of-contents selection loads the selected spine item
- Bookmarks can be added from the current reader position and reopened during the session

Bookmarks are currently session-local; persistence is intentionally left for the next iteration.


## Extended management screens
Added responsive/focus-driven screens for Connectivity, OTA/System Update, Book Manager, and Book Sync. The UI/navigation layer is wired; network, OTA download/install, and remote sync transports remain backend stubs by design.

## Service backends

Connectivity, OTA, book management, and reading-progress sync now have service backends under
`main/services/` rather than UI-only placeholders.

- Connectivity uses ESP-IDF Wi-Fi station mode, persists runtime credentials in NVS, and reports
  connection state back to the Connectivity screen.
- OTA checks a small JSON manifest asynchronously and can launch ESP-IDF HTTPS OTA. The manifest
  format is `{ "version": "1.2.3", "url": "https://.../firmware.bin" }`.
- Book Manager imports `.epub` files from `/sdcard/import` into `/sdcard/books` and removes stale
  `.part` / `.tmp` files. The library is rescanned after an operation.
- Book Sync persists its server URL and sync switches and asynchronously POSTs the current reading
  position to `<server>/v1/progress`.
- Connectivity/OTA/Sync screens poll service state every 500 ms while visible. Framebuffer diffing
  prevents an e-paper transfer when the rendered state has not changed.

Bootstrap service configuration is available in `idf.py menuconfig` under **X-Reader services**:
`XREADER_WIFI_SSID`, `XREADER_WIFI_PASSWORD`, `XREADER_OTA_MANIFEST_URL`, and
`XREADER_SYNC_SERVER`. NVS runtime configuration takes precedence where supported.

The remote *book-file* transfer protocol itself is not implemented yet; the Book Sync `BOOK FILES`
switch is persisted in preparation for the manifest/download phase. Reading-progress sync is
implemented.

### Wi-Fi pairing and text input

The Connectivity screen can now scan nearby Wi-Fi networks, select an SSID, enter a password,
connect and persist credentials, forget the saved network, and configure the sync server.
Text entry uses the reusable `ui::keyboard` component. It supports both QWERTY and T9 layouts,
touch hit-testing, and directional/select/back navigation for physical-button devices.
