# Development Workflow

## Environment

The project uses Nix flakes and pins nixpkgs to `nixos-26.05`. Enter the shell
with:

```sh
nix develop
```

The flake pins the upstream ESP-IDF source and provides host-side tools such
as CMake, Ninja, Clang, clang-format, Python, and esptool. It also packages
the official ESP-IDF 5.5.2 Xtensa and ULP tool archives through Nix.

The first shell entry creates `.nix/idf-python`, which is ignored by Git, and
installs the core ESP-IDF Python requirements there. Set
`XREADER_IDF_PYTHON_ENV_PATH` to use another local path. This environment is
host-side tooling only and adds no firmware dependency. Constraint checking is
disabled because the ESP-IDF source is stored in the Nix store rather than in
the standard mutable `$HOME/.espressif` installation tree.

## Build

```sh
nix develop
idf.py set-target esp32
idf.py reconfigure
idf.py build
```

The target is the original ESP32, not ESP32-S2, ESP32-S3, or C3.

## Format

```sh
find main -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror
```

Format only files belonging to the current change. Do not reformat unrelated
user changes.

## Flash and Monitor

```sh
idf.py flash
idf.py monitor
```

The serial device is host-specific and should be selected through the normal
ESP-IDF `-p` option. Linux users may need serial-device permissions outside
the repository configuration.

## Testing Strategy

- Build the firmware for every change that touches ESP-IDF integration.
- Run host tests for CRC32, ZIP, DEFLATE, XML, XHTML, CSS, UTF-8, and layout
  modules before hardware tests exist.
- Use diagnostic firmware for each new hardware driver.
- Test malformed and truncated EPUB input explicitly.
- Test low-memory and allocation-failure paths where feasible.
- Record hardware observations in the relevant driver documentation.

## Git Rules

- Keep commits focused by layer or milestone.
- Do not commit `build/`, `sdkconfig`, book files, or local tool state.
- Do not commit credentials, serial-port paths, or private EPUB content.
- Do not change pinned tool versions without documenting the reason.
