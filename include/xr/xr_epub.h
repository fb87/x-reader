/* xr_epub.h - bounded EPUB 2/3 container and plain-text extraction. */
#ifndef XR_EPUB_H
#define XR_EPUB_H

#include "xr_storage.h"

#include <stdint.h>

#define XR_EPUB_PATH_MAX 256u
#define XR_EPUB_ID_MAX 64u
#define XR_EPUB_TITLE_MAX 128u
#define XR_EPUB_NO_ITEM UINT16_MAX

typedef enum xr_epub_status {
    XR_EPUB_OK = 0,
    XR_EPUB_ERROR_ARGUMENT,
    XR_EPUB_ERROR_IO,
    XR_EPUB_ERROR_FORMAT,
    XR_EPUB_ERROR_UNSUPPORTED,
    XR_EPUB_ERROR_NOT_FOUND,
    XR_EPUB_ERROR_BUFFER_TOO_SMALL,
    XR_EPUB_ERROR_CAPACITY
} xr_epub_status_t;

/* Caller-owned manifest and spine storage passed to xr_epub_open(). */
typedef struct xr_epub_manifest_item {
    char id[XR_EPUB_ID_MAX];
    char href[XR_EPUB_PATH_MAX];
} xr_epub_manifest_item_t;

typedef struct xr_epub_spine_item {
    uint16_t manifest_index;
} xr_epub_spine_item_t;

typedef struct xr_epub {
    const xr_storage_t *storage;
    char package_path[XR_EPUB_PATH_MAX];
    char title[XR_EPUB_TITLE_MAX];
    xr_epub_manifest_item_t *manifest;
    uint16_t manifest_capacity;
    uint16_t manifest_count;
    xr_epub_spine_item_t *spine;
    uint16_t spine_capacity;
    uint16_t spine_count;
    void *scratch;
    uint32_t scratch_size;
} xr_epub_t;

/* A readable explanation for a status value; it never returns NULL. */
const char *xr_epub_status_string(xr_epub_status_t status);

/*
 * Parses META-INF/container.xml and the package OPF. scratch is reused for
 * archive entries and must hold both files individually. No storage is owned
 * by the EPUB object and no dynamic allocation is performed.
 */
xr_epub_status_t xr_epub_open(xr_epub_t *epub, const xr_storage_t *storage,
                              void *scratch, uint32_t scratch_size,
                              xr_epub_manifest_item_t *manifest,
                              uint16_t manifest_capacity,
                              xr_epub_spine_item_t *spine,
                              uint16_t spine_capacity);

/* Copies one archive entry into destination; entries are not NUL-terminated. */
xr_epub_status_t xr_epub_extract(const xr_epub_t *epub, const char *path,
                                 void *destination, uint32_t destination_size,
                                 uint32_t *extracted_size);

/* Resolves a spine item's href relative to the OPF path. */
xr_epub_status_t xr_epub_spine_path(const xr_epub_t *epub, uint16_t spine_index,
                                    char *path, uint32_t path_size);

/*
 * Extracts one spine XHTML document and strips markup into plain text.
 * scratch holds the XHTML source; text receives a NUL-terminated result.
 */
xr_epub_status_t xr_epub_spine_text(const xr_epub_t *epub, uint16_t spine_index,
                                    void *scratch, uint32_t scratch_size,
                                    char *text, uint32_t text_size,
                                    uint32_t *text_size_out);

#endif /* XR_EPUB_H */
