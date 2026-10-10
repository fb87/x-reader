# Development

## Reproducible Environment

The project uses a pinned Nix flake. Initial inputs are:

- nixpkgs
- `nixpkgs-esp-dev`
- ESP-IDF 5.5.2 for classic ESP32
- One exact Espressif Lua source revision
- SDL2 and zlib for the simulator

The Lua source is fetched once by Nix with a fixed hash and compiled for both
firmware and simulator. ESP-IDF Component Manager network access is disabled in
pure builds.

Expected commands are:

```bash
nix develop .#firmware
nix develop .#simulator
nix build .#firmware
nix build .#simulator
nix run .#simulator -- --library ./books
nix flake check --no-update-lock-file
```

Builds must not run ESP-IDF installers, `pip install`, or unpinned network
downloads. Firmware enables `CONFIG_APP_REPRODUCIBLE_BUILD=y`.

## Font Assets

The runtime does not load TTF files. Alegreya SemiBold is converted at build
preparation time with `tools/generate_font.py` into `core/src/font.c` and
`core/include/font.h`. The generated table contains bounded text glyphs and the
small icon range used by the Lua interface. Runtime code only consumes the
generated glyph array.

Lua exposes font sizes as the semantic values `small`, `medium`, and `large`;
the renderer maps those values to bitmap scaling. Alegreya licensing is kept
in `docs/licenses/Alegreya-OFL.txt`.

## Formatting

C uses a checked-in `.clang-format` based on LLVM style with 4-space
indentation. Lua uses a checked-in `.stylua.toml`. Both formatter versions are
pinned by Nix; editor-bundled formatters are not authoritative.

Format all project code with:

```bash
nix fmt
```

Check formatting without changing files with:

```bash
nix develop .#quality --command tools/format --check
```

`tools/format` is the single implementation used by `nix fmt`, the Git hook,
and Nix checks. It formats project-owned C headers, C sources, and Lua scripts,
while excluding fetched dependencies, generated files, build output, and
linked worktrees.

Every code edit must be formatted before compilation and tests. Editors should
use format-on-save with tools inherited from `nix develop`.

## Commit Hook

Enable the tracked hook once in the repository:

```bash
git config --local core.hooksPath .githooks
```

The pre-commit hook runs the pinned clang-format and StyLua versions. Fully
staged files are formatted and safely re-staged. Partially staged files are not
modified; if their staged content requires formatting, the commit fails and the
developer must format and select the intended hunks again.

The hook uses NUL-delimited Git output, excludes deleted files, and resolves
relative `.githooks` paths correctly from linked worktrees. CI remains
authoritative because hooks can be bypassed and hook configuration is not
cloned automatically.

## Language And Analysis

Project C code targets C11. Warnings are errors for project code. Static
analysis, sanitizer, fuzz, architecture-boundary, dependency, and Lua syntax
checks are part of the pinned Nix checks described in
[Quality and production readiness](quality.md).

## Git Worktrees

Linked worktrees live under the repository-root `worktree/` directory. The
repository commits this ignore rule:

```gitignore
/worktree/
```

Create a topic worktree with an explicit branch and start point:

```bash
git worktree add -b feature/simulator-foundation \
  worktree/simulator-foundation main
```

Run builds and recursive tools from the selected linked worktree, not from the
primary checkout. `.gitignore` affects Git only, so Nix source filtering and
other recursive tools must also exclude `worktree/`.

Inspect and remove linked worktrees through Git:

```bash
git worktree list
git worktree remove worktree/simulator-foundation
git worktree prune --dry-run --verbose
```

Do not manually delete a linked worktree directory. Avoid `git clean -fdx`
from the primary checkout because `-x` disables ignore rules.

## Build Outputs

Keep local output under `build/`, separated by target when CMake is invoked
directly:

```text
build/firmware/
build/simulator/
```

Both `build/` and `result` symlinks produced by Nix are ignored.
