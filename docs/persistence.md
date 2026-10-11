# Persistence

The application state is stored as `state.bin` under the configured state root.
The simulator places that root at `<SDCARD_ROOT>/.x-reader` by default; an
explicit `--state` path overrides it.

## Record Format

The file is a bounded binary envelope:

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII magic `XRST` |
| 4 | 4 | Little-endian format version, currently `1` |
| 8 | 4 | Little-endian payload length |
| 12 | 4 | CRC-32 of the payload |
| 16 | N | UTF-8 key/value payload |

The payload is newline-separated `key=value` data owned by Lua. The current
keys include settings and the last reading path, chapter, and page.

Records are limited to 16 KiB. Writes go to a temporary file, are flushed and
synced, and are atomically renamed over the destination. A missing record starts
with defaults. Invalid magic, version, length, or checksum is rejected rather
than applied.

The core exposes only bounded `system.state_load()` and `system.state_save()`
bindings. Lua never receives filesystem access.

The bookshelf still performs an initial bounded scan. Persisted library index
caching is a follow-up once stable book identity and invalidation rules are
defined.
