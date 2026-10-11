#ifndef READER_M5PAPER_INPUT_GT911_H
#define READER_M5PAPER_INPUT_GT911_H

#include "buses.h"

#include <esp_err.h>

enum { M5PAPER_GT911_MAX_CONTACTS = 5 };

struct m5paper_touch_point {
    uint8_t id;
    uint16_t x;
    uint16_t y;
    uint16_t size;
};

struct m5paper_gt911 {
    i2c_master_dev_handle_t device;
};

esp_err_t m5paper_gt911_init(const struct m5paper_buses* buses, struct m5paper_gt911* touch);
void m5paper_gt911_deinit(struct m5paper_gt911* touch);
esp_err_t m5paper_gt911_read(struct m5paper_gt911* touch, struct m5paper_touch_point* points,
                             uint8_t* count);

#endif
