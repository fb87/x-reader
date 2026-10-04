#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "sim_fatfs.h"
#include "xr/xr_epub.h"

#define EPUB_MANIFEST_CAPACITY 640u
#define EPUB_SPINE_CAPACITY 640u
#define EPUB_SCRATCH_SIZE (80u * 1024u)
#define EPUB_TEXT_SIZE (64u * 1024u)

static bool storage_read(void *context, uint32_t offset, void *destination, uint32_t size)
{
    FIL *file = context;
    UINT read;
    return f_lseek(file, offset) == FR_OK && f_read(file, destination, size, &read) == FR_OK &&
        read == size;
}

static bool storage_inflate(void *context, const xr_storage_t *storage, uint32_t offset,
                            uint32_t source_size, void *destination, uint32_t destination_size)
{
    uint8_t input[512];
    z_stream stream;
    uint32_t remaining = source_size;
    UINT read;
    (void)storage;
    memset(&stream, 0, sizeof(stream));
    if (f_lseek(context, offset) != FR_OK || inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;
    stream.next_out = destination;
    stream.avail_out = destination_size;
    for (;;) {
        int result;
        if (!stream.avail_in && remaining) {
            UINT chunk = remaining > sizeof(input) ? sizeof(input) : remaining;
            if (f_read(context, input, chunk, &read) != FR_OK || read != chunk) break;
            remaining -= chunk;
            stream.next_in = input;
            stream.avail_in = chunk;
        }
        result = inflate(&stream, Z_NO_FLUSH);
        if (result == Z_STREAM_END) {
            bool complete = stream.total_out == destination_size && !remaining && !stream.avail_in;
            inflateEnd(&stream);
            return complete;
        }
        if (result != Z_OK || !stream.avail_out || (!remaining && !stream.avail_in)) break;
    }
    inflateEnd(&stream);
    return false;
}

int main(int argc, char **argv)
{
    static uint8_t scratch[EPUB_SCRATCH_SIZE];
    static char text[EPUB_TEXT_SIZE];
    static xr_epub_manifest_item_t manifest[EPUB_MANIFEST_CAPACITY];
    static xr_epub_spine_item_t spine[EPUB_SPINE_CAPACITY];
    FIL file;
    xr_storage_t storage;
    xr_epub_t epub;
    xr_epub_status_t status;
    const char *path = argc == 2 ? argv[1] : "/home/dao/data/sample.epub";

    sim_fatfs_mount(".");
    if (f_open(&file, path, FA_READ) != FR_OK) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    storage = (xr_storage_t) { &file, f_size(&file), storage_read, storage_inflate };
    status = xr_epub_open(&epub, &storage, scratch, sizeof(scratch), manifest,
                          EPUB_MANIFEST_CAPACITY, spine, EPUB_SPINE_CAPACITY);
    if (status == XR_EPUB_OK)
        status = xr_epub_spine_text(&epub, 1, scratch, sizeof(scratch), text, sizeof(text), NULL);
    f_close(&file);
    if (status != XR_EPUB_OK || epub.manifest_count != 602 || epub.spine_count != 596 || !text[0]) {
        fprintf(stderr, "EPUB import failed: %s (manifest=%u spine=%u text=%u)\n",
                xr_epub_status_string(status), epub.manifest_count, epub.spine_count,
                (unsigned)(uint8_t)text[0]);
        return 1;
    }
    printf("EPUB import passed: manifest=%u spine=%u\n", epub.manifest_count, epub.spine_count);
    return 0;
}
