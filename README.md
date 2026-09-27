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
