#include "buses.h"

#include "board_config.h"

#include "driver/gpio.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* const TAG = "x-reader-bus";

static esp_err_t configure_control_gpio(gpio_num_t pin) {
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << (unsigned)pin,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&config);
}

esp_err_t m5paper_buses_init(struct m5paper_buses* buses) {
    if (buses == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(buses, 0, sizeof(*buses));

    esp_err_t status = configure_control_gpio(M5PAPER_DISPLAY_SPI_CS);
    if (status != ESP_OK) {
        return status;
    }
    status = configure_control_gpio(M5PAPER_STORAGE_SPI_CS);
    if (status != ESP_OK) {
        return status;
    }
    status = configure_control_gpio(M5PAPER_POWER_ENABLE);
    if (status != ESP_OK) {
        return status;
    }
    status = configure_control_gpio(M5PAPER_EXTERNAL_POWER_ENABLE);
    if (status != ESP_OK) {
        return status;
    }
    status = configure_control_gpio(M5PAPER_DISPLAY_POWER_ENABLE);
    if (status != ESP_OK) {
        return status;
    }
    status = gpio_set_level(M5PAPER_DISPLAY_SPI_CS, 1);
    if (status != ESP_OK) {
        return status;
    }
    status = gpio_set_level(M5PAPER_POWER_ENABLE, 1);
    if (status != ESP_OK) {
        return status;
    }
    status = gpio_set_level(M5PAPER_EXTERNAL_POWER_ENABLE, 1);
    if (status != ESP_OK) {
        return status;
    }
    status = gpio_set_level(M5PAPER_DISPLAY_POWER_ENABLE, 1);
    if (status != ESP_OK) {
        return status;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    status = gpio_set_level(M5PAPER_STORAGE_SPI_CS, 1);
    if (status != ESP_OK) {
        return status;
    }

    const spi_bus_config_t spi_config = {
        .mosi_io_num = M5PAPER_DISPLAY_SPI_MOSI,
        .miso_io_num = M5PAPER_DISPLAY_SPI_MISO,
        .sclk_io_num = M5PAPER_DISPLAY_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    status = spi_bus_initialize(VSPI_HOST, &spi_config, SPI_DMA_CH_AUTO);
    if (status != ESP_OK) {
        return status;
    }

    const spi_device_interface_config_t display_config = {
        .command_bits = 0,
        .address_bits = 0,
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = M5PAPER_DISPLAY_SPI_CS,
        .queue_size = 1,
    };
    status = spi_bus_add_device(VSPI_HOST, &display_config, &buses->display);
    if (status != ESP_OK) {
        spi_bus_free(VSPI_HOST);
        return status;
    }

    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = M5PAPER_I2C_SDA,
        .scl_io_num = M5PAPER_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    status = i2c_new_master_bus(&i2c_config, &buses->i2c);
    if (status != ESP_OK) {
        spi_bus_remove_device(buses->display);
        buses->display = NULL;
        spi_bus_free(VSPI_HOST);
        return status;
    }

    ESP_LOGI(TAG, "shared SPI and I2C buses initialized");
    return ESP_OK;
}

void m5paper_buses_deinit(struct m5paper_buses* buses) {
    if (buses == NULL) {
        return;
    }
    if (buses->i2c != NULL) {
        (void)i2c_del_master_bus(buses->i2c);
        buses->i2c = NULL;
    }
    if (buses->display != NULL) {
        (void)spi_bus_remove_device(buses->display);
        buses->display = NULL;
    }
    (void)spi_bus_free(VSPI_HOST);
}
