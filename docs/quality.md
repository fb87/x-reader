# Quality And Production Readiness

Production readiness is a design input from the first skeleton. Features are
not considered complete when they only work on valid input or on the simulator.

## Build And Dependencies

- Pin nixpkgs, `nixpkgs-esp-dev`, ESP-IDF, Lua, SDL2, zlib, and build tools.
- Disable network dependency resolution in pure builds.
- Enable `CONFIG_APP_REPRODUCIBLE_BUILD=y` for firmware.
- Keep firmware runtime dependencies limited to ESP-IDF and the approved Lua
  component.
- Keep SDL2 and zlib confined to the simulator platform.
- Treat formatters, analyzers, sanitizers, and fuzzers as build-time tools only.
- Record each dependency's source, revision, hash, license, and target usage.
- Fail checks on unpinned or incompatible dependencies.

Two clean release builds should produce identical artifacts. Any unavoidable
metadata difference must be identified and documented rather than ignored.

## Formatting

C and headers use the pinned clang-format version and `.clang-format`, based on
LLVM style with 4-space indentation. Lua uses the pinned StyLua version and
`.stylua.toml`.

Formatting is applied at three points:

1. Editors should format on save using tools from `nix develop`.
2. The tracked pre-commit hook safely formats fully staged files.
3. `nix flake check` rejects any committed formatting drift.

The CI check is authoritative because local hooks can be bypassed.

## Compiler Baseline

Project C code targets C11. Initial warnings include:

```text
-Wall
-Wextra
-Wpedantic
-Werror
-Wconversion
-Wsign-conversion
-Wshadow
-Wformat=2
-Wundef
-Wvla
-Wstrict-prototypes
-Wmissing-prototypes
-Wcast-align
-Wwrite-strings
```

Warnings apply to project code, not fetched or system headers. The host core and
simulator build with pinned GCC and Clang; firmware builds with the pinned
Xtensa compiler.

## Static Analysis

Checks include:

- clang-tidy with a checked-in, intentionally small rule set
- A host static analyzer for host-buildable core code
- Lua syntax validation for every script
- An architecture check preventing platform headers and types from entering
  public core headers
- A firmware link-map check for unapproved dependencies
- A simulator closure check for unexpected runtime dependencies

New unsuppressed findings fail the check. Suppressions require a local reason
and must be narrow enough not to hide unrelated findings.

## Sanitized Builds

Host tests run under AddressSanitizer and UndefinedBehaviorSanitizer, including
leak detection where supported. Sanitizer failures, leaks, hangs, and unexpected
termination fail checks.

Release firmware does not use host sanitizers. A separate firmware diagnostic
profile enables ESP-IDF heap poisoning, stack protections, assertions, and
verbose resource metrics.

## Untrusted EPUB Input

Limits are defined before parser implementation and are configurable in one
place. They include:

- Archive size and entry count
- Compressed and expanded bytes per entry and publication
- Compression expansion ratio
- Path length, path depth, and normalized-name count
- XML depth, attributes per element, and token lengths
- Manifest, spine, navigation, selector, and link counts
- PNG dimensions, row bytes, and total decoded work
- Lua allocator budget
- Parser operation or deadline budget

Parsers use checked arithmetic before offsets, allocation, multiplication, and
pointer movement. They reject traversal, absolute paths, ambiguous separators,
duplicate normalized names, overlapping ZIP records, truncation, cycles,
unsupported encryption, external XML entities, and excessive expansion.

Errors distinguish malformed input, unsupported features, resource exhaustion,
cancellation, timeout, and platform I/O failure. One bad book must not prevent
the bookshelf from opening other books.

## Fuzzing

Host fuzz targets cover:

- ZIP metadata, path normalization, and entry streams
- XML and XHTML tokenization
- EPUB container, package, and navigation parsing
- PNG decoding
- UTF-8 normalization and layout input
- Lua binding argument validation
- Hot-reload snapshot validation

Fuzz builds use libFuzzer with ASan and UBSan. Per-change checks replay the
regression corpus and run short bounded smoke sessions. Scheduled jobs run
longer sessions. Every discovered crash is minimized and retained as a fixture.

Memory and wall-clock limits turn hangs and decompression bombs into explicit
test failures.

## Tests

The project uses a small in-tree C test harness instead of adding a firmware
runtime dependency. Required groups include:

- Checked arithmetic and path normalization
- Streams, parser state machines, and error recovery
- Graphics clipping, dirty rectangles, and coordinate transforms
- Persistence encoding and interrupted-write recovery
- Valid, malformed, truncated, and boundary ZIP/XML/EPUB/PNG fixtures
- Matching zlib and ROM `tinfl` behavior against shared fixtures
- Lua API types, errors, stale userdata, restricted libraries, and limits
- Transactional hot reload success, syntax failure, initialization failure,
  snapshot rejection, and rollback
- EPUB-to-layout-to-framebuffer integration
- SDL dummy-driver smoke tests
- CPU-side framebuffer golden comparisons

Tests must not depend on network access, a home directory, locale, timezone,
filesystem iteration order, renderer pixels, or uncontrolled wall-clock timing.

## Memory And Timeouts

Every public interface documents ownership, lifetime, cleanup, allocation
ceiling, callback validity, and thread assumptions.

- Cleanup is safe after partial initialization.
- Queues, logs, Lua memory, parser buffers, pages, and caches are bounded.
- Host tests inject allocation and I/O failure at practical failure points.
- SPI, I2C, display readiness, storage, event waits, parsers, and Lua callbacks
  have deadlines or instruction budgets.
- Deadlines use monotonic time and wrap-safe elapsed calculations.
- No platform operation waits forever.
- Firmware records peak heap, largest free block, PSRAM use, and stack
  high-water marks against explicit budgets.

## Persistence And Power Loss

Persistent records include a schema version, sequence number, length, checksum,
stable book identity, logical position, and layout-relevant settings.

Writes use two slots or a journal: write, synchronize, read back, validate, and
then publish the new sequence or commit marker. The only valid record is never
overwritten in place.

Simulator tests interrupt writes at every operation boundary and require
recovery to either the previous or new complete state. Position writes are
rate-limited for flash wear and attempted during explicit sleep or shutdown.

## Observability

One bounded logging interface provides severity, subsystem, stable event ID,
result code, and monotonic timestamp. Diagnostics include:

- Source revision, build identity, dependency versions, and feature profile
- Reset reason and active SDL video backend
- Configured resource limits
- Parser failures with book identity, entry, and location
- Heap, PSRAM, stack, refresh duration, storage errors, and timeout counts

Raw book text is not logged by default. Critical error paths must not require
unbounded allocation.

## Build Profiles

| Profile | Purpose | Required behavior |
| --- | --- | --- |
| `dev` | Local work | Symbols, assertions, warnings as errors, hot reload |
| `test` | Deterministic checks | Test hooks, fault injection, controlled paths/time |
| `sanitize` | Host memory safety | ASan, UBSan, and leak checks |
| `fuzz` | Parser hardening | libFuzzer, sanitizers, and resource limits |
| `firmware-diagnostic` | Device debugging | Heap poisoning, stack checks, verbose metrics |
| `release` | Shipping | Optimized, reproducible, invariant checks retained |

Release behavior is explicit and is not inferred only from `NDEBUG`.

## Change Gates

Every accepted change must pass applicable pinned checks:

1. clang-format and StyLua checks
2. Firmware and simulator builds with warnings as errors
3. GCC and Clang host builds
4. clang-tidy, static analysis, and Lua syntax checks
5. Unit, fixture, integration, and SDL headless tests
6. ASan and UBSan tests
7. Bounded fuzz corpus replay
8. Dependency, license, closure, and link-map checks
9. Reproducibility and artifact-size regression checks

Before validated hardware is available, artifacts are labeled simulator or
skeleton builds rather than production firmware.

## Hardware Release Gates

A production firmware release additionally requires:

- Verified board revision, pin map, power sequence, and peripheral identity
- Boot, PSRAM, SD, display, touch, controls, sleep, and wake tests
- Display orientation, grayscale, DMA, timeout, and recovery validation
- Measured RAM, PSRAM, fragmentation, stack, and performance margins
- Repeated power-cut persistence testing
- SD removal and reinsertion testing during all relevant operations
- Missing, slow, and wedged peripheral fault injection
- Long-duration navigation and repeated open/close testing
- Measured refresh ghosting and power consumption
- Watchdog, brownout, and reset-reason validation
- No unresolved critical or high-severity parser findings

Release artifacts include firmware binaries, ELF/debug symbols, map file,
configuration, source revision, lock-file identity, dependency/license
inventory, checksums, and qualification results.
