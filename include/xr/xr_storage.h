/* xr_storage.h - caller-provided, random-access byte storage. */
#ifndef XR_STORAGE_H
#define XR_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct xr_storage xr_storage_t;

/* Read exactly size bytes at offset. Return false for an I/O failure. */
typedef bool (*xr_storage_read_fn)(void *context, uint32_t offset,
                                   void *destination, uint32_t size);

/*
 * Inflate a raw RFC 1951 DEFLATE stream directly from storage.  ZIP method 8
 * entries use this callback.  destination_size is the expected uncompressed
 * size, so a port must fail if the stream does not produce exactly that size.
 */
typedef bool (*xr_storage_inflate_raw_fn)(void *context,
                                          const xr_storage_t *storage,
                                          uint32_t source_offset,
                                          uint32_t source_size,
                                          void *destination,
                                          uint32_t destination_size);

struct xr_storage {
    void *context;
    uint32_t size;
    xr_storage_read_fn read;
    xr_storage_inflate_raw_fn inflate_raw;
};

#endif /* XR_STORAGE_H */
