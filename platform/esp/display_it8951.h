#ifndef READER_M5PAPER_DISPLAY_IT8951_H
#define READER_M5PAPER_DISPLAY_IT8951_H

#include "buses.h"

#include "driver/gpio.h"

#include <esp_err.h>

struct m5paper_it8951_info {
    uint16_t panel_width;
    uint16_t panel_height;
    uint32_t image_buffer_address;
};

struct m5paper_it8951 {
    spi_device_handle_t device;
    gpio_num_t busy_pin;
    uint8_t* row_chunk_buffer;
};

enum m5paper_it8951_update_mode {
    M5PAPER_IT8951_UPDATE_DU = 1,
    M5PAPER_IT8951_UPDATE_GC16 = 2,
    M5PAPER_IT8951_UPDATE_GL16 = 3,
};

esp_err_t m5paper_it8951_init(const struct m5paper_buses* buses, struct m5paper_it8951* display);
void m5paper_it8951_deinit(struct m5paper_it8951* display);
esp_err_t m5paper_it8951_system_info(struct m5paper_it8951* display,
                                     struct m5paper_it8951_info* info);
esp_err_t m5paper_it8951_show_diagnostic(struct m5paper_it8951* display,
                                         const struct m5paper_it8951_info* info);
esp_err_t m5paper_it8951_present(struct m5paper_it8951* display,
                                 const struct m5paper_it8951_info* info, const uint8_t* framebuffer,
                                 uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                 enum m5paper_it8951_update_mode mode);

#endif
