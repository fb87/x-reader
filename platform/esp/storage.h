#ifndef READER_M5PAPER_STORAGE_H
#define READER_M5PAPER_STORAGE_H

#include "../../core/include/platform.h"

#include <esp_err.h>
#include <sdmmc_cmd.h>

struct m5paper_storage {
    sdmmc_card_t* card;
    bool mounted;
};

esp_err_t m5paper_storage_mount(struct m5paper_storage* storage);
void m5paper_storage_unmount(struct m5paper_storage* storage);
enum platform_status m5paper_storage_list(void* opaque, const char* root, const char* relative,
                                          struct platform_entry* entries, uint32_t capacity,
                                          uint32_t* count);
enum platform_status m5paper_storage_open_archive(void* opaque, const char* root, const char* path,
                                                  struct platform_archive* archive);
void m5paper_storage_close_archive(void* opaque, struct platform_archive* archive);

#endif
