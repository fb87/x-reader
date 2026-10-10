# Parser Progress

## Current Slice

`core/src/zip.c` now provides a bounded ZIP central-directory index through
`core/include/zip.h`.

It currently:

- Locates the End of Central Directory record within the bounded ZIP trailer
- Rejects split/multi-disk archives
- Indexes central-directory entries without loading member data
- Validates archive bounds and checked sizes
- Rejects encrypted entries and compression methods other than stored entries
- Rejects absolute, traversal, ambiguous, empty, and duplicate virtual paths
- Accepts safe directory entries with trailing `/`
- Enforces entry-count, name-length, per-entry, and total expansion limits
- Exposes stable entry metadata without exposing platform file types
- Reads stored entries with local-header validation and CRC-32 verification
- Limits whole-entry reads to a small metadata budget; large XHTML and image
  entries require a streaming entry API

The package slice now discovers the OPF path, title, creator, manifest, and linear
spine for the supported reflowable EPUB profile. XHTML layout, navigation, and
firmware DEFLATE streaming, and full streaming chapter reads remain separate
increments.

## Boundary

The ZIP reader receives a portable random-access source:

```c
struct zip_source {
    void *context;
    uint64_t size;
    enum zip_status (*read_at)(void *, uint64_t, void *, size_t);
    enum zip_status (*inflate_raw)(void *, uint64_t, size_t, void *, size_t);
};
```

The source may be backed by FatFs on firmware or a host file on the simulator.
Neither backend type crosses the core API.

## Next Steps

1. Add local-header and data-range validation.
2. Add a bounded streaming entry reader; never inflate a chapter into one
   allocation.
3. Add platform DEFLATE callbacks: ROM `tinfl` on firmware and zlib on SDL.
4. Add `container.xml` and OPF streaming parsers.
5. Add shared valid, malformed, truncated, duplicate-path, and ZIP-bomb
   fixtures.

The simulator `zip-probe` can inspect a real publication from the configured
SD-card corpus:

```bash
nix develop .#simulator --command cmake --build /tmp/x-reader-zip-build --target zip-probe
nix develop .#simulator --command /tmp/x-reader-zip-build/zip-probe \
  "/home/dao/data/sdcard/10k ePub/Khoa học/Phân tâm học nhập môn - Sigmund Freud.epub"
```
