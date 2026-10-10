#include "epub.h"
#include "zip.h"

#include <zlib.h>

#include <stdio.h>
#include <stdlib.h>

static enum zip_status file_read_at(void* context, uint64_t offset, void* buffer, size_t length) {
    FILE* file = context;
    if (fseek(file, (long)offset, SEEK_SET) != 0 || fread(buffer, 1U, length, file) != length) {
        return ZIP_IO_ERROR;
    }
    return ZIP_OK;
}

static enum zip_status file_inflate(void* context, uint64_t offset, size_t compressed_size,
                                    void* output, size_t output_size) {
    FILE* file = context;
    uint8_t* compressed = malloc(compressed_size == 0U ? 1U : compressed_size);
    if (compressed == NULL)
        return ZIP_OUT_OF_MEMORY;
    const enum zip_status status = file_read_at(file, offset, compressed, compressed_size);
    if (status != ZIP_OK) {
        free(compressed);
        return status;
    }
    z_stream stream = {0};
    stream.next_in = compressed;
    stream.avail_in = (uInt)compressed_size;
    stream.next_out = output;
    stream.avail_out = (uInt)output_size;
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK || inflate(&stream, Z_FINISH) != Z_STREAM_END ||
        stream.total_out != output_size) {
        inflateEnd(&stream);
        free(compressed);
        return ZIP_MALFORMED;
    }
    inflateEnd(&stream);
    free(compressed);
    return ZIP_OK;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s BOOK.epub\n", argv[0]);
        return 2;
    }
    FILE* file = fopen(argv[1], "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0)
        return 1;
    const long end = ftell(file);
    if (end < 0)
        return 1;
    const struct zip_source source = {
        .context = file,
        .size = (uint64_t)end,
        .read_at = file_read_at,
        .inflate_raw = file_inflate,
    };
    const struct zip_limits limits = {
        .max_entries = 4096U,
        .max_name_bytes = 1024U,
        .max_entry_uncompressed_bytes = 16U * 1024U * 1024U,
        .max_total_uncompressed_bytes = 128U * 1024U * 1024U,
        .max_read_all_bytes = 4U * 1024U * 1024U,
    };
    struct zip_archive* archive = NULL;
    const enum zip_status open_status = zip_open(&source, &limits, &archive);
    if (open_status != ZIP_OK) {
        fprintf(stderr, "zip_open failed: %d\n", open_status);
        return 1;
    }
    printf("entries=%u\n", zip_entry_count(archive));
    uint32_t index = 0U;
    if (zip_entry_find(archive, "mimetype", &index)) {
        uint8_t* data = NULL;
        size_t length = 0U;
        if (zip_entry_read_all(archive, index, &data, &length) == ZIP_OK) {
            printf("mimetype=%.*s\n", (int)length, data);
            zip_data_free(data);
        }
    }
    if (zip_entry_find(archive, "META-INF/container.xml", &index)) {
        uint8_t* data = NULL;
        size_t length = 0U;
        const enum zip_status read_status = zip_entry_read_all(archive, index, &data, &length);
        printf("container_status=%d bytes=%zu\n", read_status, length);
        zip_data_free(data);
    }
    struct epub_book* book = NULL;
    const struct epub_limits epub_limits = {
        .max_metadata_bytes = 64U * 1024U,
        .max_manifest_items = 1024U,
        .max_spine_items = 1024U,
        .max_text_bytes = 256U,
    };
    const enum epub_status epub_result = epub_open(archive, &epub_limits, &book);
    printf("epub_status=%d title=%s creator=%s spine=%u opf=%s\n", epub_result,
           epub_title(book) == NULL ? "" : epub_title(book),
           epub_creator(book) == NULL ? "" : epub_creator(book), epub_spine_count(book),
           epub_opf_path(book) == NULL ? "" : epub_opf_path(book));
    if (epub_result == EPUB_OK && epub_spine_count(book) > 0U) {
        struct epub_spine_item item;
        uint8_t* text = NULL;
        size_t text_length = 0U;
        const enum epub_status spine_status = epub_spine_get(book, 0U, &item);
        const enum epub_status text_status = epub_spine_text(book, 0U, &text, &text_length);
        printf("spine0_status=%d href=%s text_status=%d text_bytes=%zu\n", spine_status,
               spine_status == EPUB_OK ? item.href : "", text_status, text_length);
        epub_text_free(text);
    }
    epub_close(book);
    zip_close(archive);
    fclose(file);
    return 0;
}
