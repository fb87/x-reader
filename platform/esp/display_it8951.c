#include "display_it8951.h"

#include "board_config.h"

#include "driver/gpio.h"

#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

enum {
    IT8951_SPI_COMMAND = 0x6000,
    IT8951_SPI_DATA = 0x0000,
    IT8951_SYSTEM_RUN = 0x0001,
    IT8951_REGISTER_WRITE = 0x0011,
    IT8951_LOAD_IMAGE_AREA = 0x0021,
    IT8951_LOAD_IMAGE_END = 0x0022,
    IT8951_DISPLAY_BUFFER_AREA = 0x0037,
    IT8951_VCOM = 0x0039,
    IT8951_SPI_READ = 0x1000,
    IT8951_GET_SYSTEM_INFO = 0x0302,
    IT8951_SYSTEM_INFO_WORDS = 20,
    IT8951_BUSY_TIMEOUT_MS = 3000,
    IT8951_I80CPCR = 0x0004,
    IT8951_LISAR = 0x0208,
    IT8951_4BPP_BIG_ENDIAN_ROTATE_90 = 0x0121,
    IT8951_UPDATE_GC16 = 2,
    IT8951_VCOM_MILLIVOLTS = 2300,
    DIAGNOSTIC_BAND_HEIGHT = 60,
    IT8951_MAX_LOGICAL_WIDTH = 540,
};

static const char* const TAG = "x-reader-display";

static esp_err_t wait_ready(const struct m5paper_it8951* display) {
    const int64_t deadline = esp_timer_get_time() + (int64_t)IT8951_BUSY_TIMEOUT_MS * 1000;
    while (gpio_get_level(display->busy_pin) == 0) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(1000);
    }
    return ESP_OK;
}

static esp_err_t transfer_locked(struct m5paper_it8951* display, const uint8_t* transmit,
                                 uint8_t* receive, size_t length, bool keep_cs) {
    if (display == NULL || display->device == NULL || transmit == NULL || length == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    spi_transaction_t transaction = {
        .length = length * 8U,
        .rxlength = length * 8U,
        .tx_buffer = transmit,
        .rx_buffer = receive,
        .flags = keep_cs ? SPI_TRANS_CS_KEEP_ACTIVE : 0,
    };
    return spi_device_polling_transmit(display->device, &transaction);
}

static esp_err_t send_cycle(struct m5paper_it8951* display, uint16_t preamble, uint16_t word) {
    const uint8_t preamble_bytes[] = {(uint8_t)(preamble >> 8U), (uint8_t)(preamble & 0xffU)};
    const uint8_t word_bytes[] = {(uint8_t)(word >> 8U), (uint8_t)(word & 0xffU)};
    esp_err_t status = spi_device_acquire_bus(display->device, portMAX_DELAY);
    const bool acquired = status == ESP_OK;
    if (status == ESP_OK) {
        status = wait_ready(display);
    }
    if (status == ESP_OK) {
        status = transfer_locked(display, preamble_bytes, NULL, sizeof(preamble_bytes), true);
    }
    if (status == ESP_OK) {
        status = wait_ready(display);
    }
    if (status == ESP_OK) {
        status = transfer_locked(display, word_bytes, NULL, sizeof(word_bytes), false);
    }
    if (acquired) {
        spi_device_release_bus(display->device);
    }
    return status;
}

static esp_err_t send_command(struct m5paper_it8951* display, uint16_t command) {
    return send_cycle(display, IT8951_SPI_COMMAND, command);
}

static esp_err_t send_data(struct m5paper_it8951* display, uint16_t data) {
    return send_cycle(display, IT8951_SPI_DATA, data);
}

static esp_err_t send_arguments(struct m5paper_it8951* display, uint16_t command,
                                const uint16_t* arguments, size_t count) {
    esp_err_t status = send_command(display, command);
    for (size_t index = 0; status == ESP_OK && index < count; ++index) {
        status = send_data(display, arguments[index]);
    }
    return status;
}

static esp_err_t write_register(struct m5paper_it8951* display, uint16_t address, uint16_t value) {
    const uint16_t arguments[] = {address, value};
    return send_arguments(display, IT8951_REGISTER_WRITE, arguments,
                          sizeof(arguments) / sizeof(arguments[0]));
}

static esp_err_t write_pixel_word(struct m5paper_it8951* display, uint16_t value) {
    const uint8_t transmit[] = {0, 0, (uint8_t)(value >> 8U), (uint8_t)(value & 0xffU)};
    return transfer_locked(display, transmit, NULL, sizeof(transmit), false);
}

static esp_err_t write_pixel_row(struct m5paper_it8951* display, const uint8_t* framebuffer,
                                 size_t row_offset, uint16_t x, uint16_t width) {
    uint8_t transmit[2U + IT8951_MAX_LOGICAL_WIDTH / 2U];
    transmit[0] = 0U;
    transmit[1] = 0U;
    const size_t row_bytes = (size_t)width / 2U;
    memcpy(&transmit[2], &framebuffer[row_offset + (size_t)x / 2U], row_bytes);
    return transfer_locked(display, transmit, NULL, 2U + row_bytes, false);
}

esp_err_t m5paper_it8951_init(const struct m5paper_buses* buses, struct m5paper_it8951* display) {
    if (buses == NULL || buses->display == NULL || display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    display->device = buses->display;
    display->busy_pin = M5PAPER_DISPLAY_BUSY;
    const gpio_config_t busy_config = {
        .pin_bit_mask = 1ULL << (unsigned)display->busy_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t status = gpio_config(&busy_config);
    if (status != ESP_OK) {
        display->device = NULL;
    }
    return status;
}

void m5paper_it8951_deinit(struct m5paper_it8951* display) {
    if (display != NULL) {
        display->device = NULL;
    }
}

esp_err_t m5paper_it8951_system_info(struct m5paper_it8951* display,
                                     struct m5paper_it8951_info* info) {
    if (display == NULL || display->device == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t status = send_cycle(display, IT8951_SPI_COMMAND, IT8951_GET_SYSTEM_INFO);
    if (status != ESP_OK) {
        return status;
    }

    const uint8_t read_preamble[] = {(uint8_t)(IT8951_SPI_READ >> 8U),
                                     (uint8_t)(IT8951_SPI_READ & 0xffU)};
    const uint8_t dummy[] = {0, 0};
    uint8_t transmit[IT8951_SYSTEM_INFO_WORDS * sizeof(uint16_t)] = {0};
    uint8_t response[sizeof(transmit)] = {0};
    status = spi_device_acquire_bus(display->device, portMAX_DELAY);
    const bool acquired = status == ESP_OK;
    if (status == ESP_OK) {
        status = wait_ready(display);
    }
    if (status == ESP_OK) {
        status = transfer_locked(display, read_preamble, NULL, sizeof(read_preamble), true);
    }
    if (status == ESP_OK) {
        status = wait_ready(display);
    }
    if (status == ESP_OK) {
        status = transfer_locked(display, dummy, NULL, sizeof(dummy), true);
    }
    if (status == ESP_OK) {
        status = wait_ready(display);
    }
    if (status == ESP_OK) {
        status = transfer_locked(display, transmit, response, sizeof(response), false);
    }
    if (acquired) {
        spi_device_release_bus(display->device);
    }
    if (status != ESP_OK) {
        return status;
    }

    uint16_t words[IT8951_SYSTEM_INFO_WORDS] = {0};
    for (size_t index = 0; index < IT8951_SYSTEM_INFO_WORDS; ++index) {
        const size_t offset = index * sizeof(uint16_t);
        words[index] = (uint16_t)response[offset] << 8U;
        words[index] |= response[offset + 1U];
    }
    info->panel_width = words[0];
    info->panel_height = words[1];
    info->image_buffer_address = ((uint32_t)words[3] << 16U) | words[2];
    if (info->panel_width == 0U || info->panel_height == 0U || info->image_buffer_address == 0U) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGI(TAG, "IT8951 panel %ux%u, buffer 0x%08x", info->panel_width, info->panel_height,
             (unsigned)info->image_buffer_address);
    return ESP_OK;
}

esp_err_t m5paper_it8951_show_diagnostic(struct m5paper_it8951* display,
                                         const struct m5paper_it8951_info* info) {
    if (display == NULL || display->device == NULL || info == NULL ||
        (info->panel_height % 4U) != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint16_t logical_width = info->panel_height;
    const uint16_t logical_height = info->panel_width;

    esp_err_t status = send_command(display, IT8951_SYSTEM_RUN);
    if (status == ESP_OK) {
        status = write_register(display, IT8951_I80CPCR, 1);
    }
    const uint16_t vcom[] = {1, IT8951_VCOM_MILLIVOLTS};
    if (status == ESP_OK) {
        status = send_arguments(display, IT8951_VCOM, vcom, sizeof(vcom) / sizeof(vcom[0]));
    }
    if (status == ESP_OK) {
        status =
            write_register(display, IT8951_LISAR, (uint16_t)(info->image_buffer_address & 0xffffU));
    }
    if (status == ESP_OK) {
        status = write_register(display, IT8951_LISAR + 2U,
                                (uint16_t)(info->image_buffer_address >> 16U));
    }

    const uint16_t load_area[] = {
        IT8951_4BPP_BIG_ENDIAN_ROTATE_90, 0, 0, logical_width, logical_height,
    };
    if (status == ESP_OK) {
        status = send_arguments(display, IT8951_LOAD_IMAGE_AREA, load_area,
                                sizeof(load_area) / sizeof(load_area[0]));
    }
    for (uint16_t row = 0; status == ESP_OK && row < logical_height; ++row) {
        status = wait_ready(display);
        if (status != ESP_OK) {
            break;
        }
        status = spi_device_acquire_bus(display->device, portMAX_DELAY);
        const bool acquired = status == ESP_OK;
        const uint16_t color = ((row / DIAGNOSTIC_BAND_HEIGHT) & 1U) == 0U ? 0xffffU : 0x0000U;
        for (uint16_t column = 0; status == ESP_OK && column < logical_width; column += 4U) {
            status = write_pixel_word(display, color);
        }
        if (acquired) {
            spi_device_release_bus(display->device);
        }
        vTaskDelay(1);
    }
    if (status == ESP_OK) {
        status = send_command(display, IT8951_LOAD_IMAGE_END);
    }

    const uint16_t refresh[] = {
        0,
        0,
        info->panel_width,
        info->panel_height,
        IT8951_UPDATE_GC16,
        (uint16_t)(info->image_buffer_address & 0xffffU),
        (uint16_t)(info->image_buffer_address >> 16U),
    };
    if (status == ESP_OK) {
        status = send_arguments(display, IT8951_DISPLAY_BUFFER_AREA, refresh,
                                sizeof(refresh) / sizeof(refresh[0]));
    }
    if (status == ESP_OK) {
        ESP_LOGI(TAG, "portrait diagnostic %ux%u submitted with GC16 refresh", logical_width,
                 logical_height);
    }
    return status;
}

esp_err_t m5paper_it8951_present(struct m5paper_it8951* display,
                                 const struct m5paper_it8951_info* info, const uint8_t* framebuffer,
                                 uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                 enum m5paper_it8951_update_mode mode) {
    if (display == NULL || display->device == NULL || info == NULL || framebuffer == NULL ||
        width == 0U || height == 0U || (x % 4U) != 0U || (width % 4U) != 0U ||
        x + width > info->panel_height || y + height > info->panel_width) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint16_t logical_width = info->panel_height;
    esp_err_t status = send_command(display, IT8951_SYSTEM_RUN);
    if (status == ESP_OK) {
        status = write_register(display, IT8951_I80CPCR, 1);
    }
    const uint16_t vcom[] = {1, IT8951_VCOM_MILLIVOLTS};
    if (status == ESP_OK) {
        status = send_arguments(display, IT8951_VCOM, vcom, sizeof(vcom) / sizeof(vcom[0]));
    }
    if (status == ESP_OK) {
        status =
            write_register(display, IT8951_LISAR, (uint16_t)(info->image_buffer_address & 0xffffU));
    }
    if (status == ESP_OK) {
        status = write_register(display, IT8951_LISAR + 2U,
                                (uint16_t)(info->image_buffer_address >> 16U));
    }
    const uint16_t load_area[] = {IT8951_4BPP_BIG_ENDIAN_ROTATE_90, x, y, width, height};
    if (status == ESP_OK) {
        status = send_arguments(display, IT8951_LOAD_IMAGE_AREA, load_area,
                                sizeof(load_area) / sizeof(load_area[0]));
    }
    for (uint16_t row = y; status == ESP_OK && row < y + height; ++row) {
        status = wait_ready(display);
        if (status != ESP_OK) {
            break;
        }
        status = spi_device_acquire_bus(display->device, portMAX_DELAY);
        const bool acquired = status == ESP_OK;
        const size_t row_offset = (size_t)row * (logical_width / 2U);
        if (status == ESP_OK) {
            status = write_pixel_row(display, framebuffer, row_offset, x, width);
        }
        if (acquired) {
            spi_device_release_bus(display->device);
        }
    }
    if (status == ESP_OK) {
        status = send_command(display, IT8951_LOAD_IMAGE_END);
    }
    const uint16_t refresh[] = {y,
                                (uint16_t)(info->panel_height - width - x),
                                height,
                                width,
                                (uint16_t)mode,
                                (uint16_t)(info->image_buffer_address & 0xffffU),
                                (uint16_t)(info->image_buffer_address >> 16U)};
    if (status == ESP_OK) {
        status = send_arguments(display, IT8951_DISPLAY_BUFFER_AREA, refresh,
                                sizeof(refresh) / sizeof(refresh[0]));
    }
    return status;
}
