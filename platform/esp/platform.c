#include "board_config.h"
#include "buses.h"
#include "display_it8951.h"
#include "input_gt911.h"
#include "m5paper_platform.h"

#include <esp_log.h>

static const char* const TAG = "x-reader";

esp_err_t esp_platform_init(void) {
    struct m5paper_buses buses;
    ESP_LOGI(TAG, "M5Paper bring-up requested");
    ESP_LOGI(TAG, "shared SPI: MISO=%d MOSI=%d SCLK=%d", M5PAPER_DISPLAY_SPI_MISO,
             M5PAPER_DISPLAY_SPI_MOSI, M5PAPER_DISPLAY_SPI_SCLK);
    ESP_LOGI(TAG, "display CS=%d, storage CS=%d", M5PAPER_DISPLAY_SPI_CS, M5PAPER_STORAGE_SPI_CS);
    ESP_LOGI(TAG, "GT911 I2C: SDA=%d SCL=%d INT=%d", M5PAPER_I2C_SDA, M5PAPER_I2C_SCL,
             M5PAPER_TOUCH_INT);
    ESP_LOGI(TAG, "IT8951 busy=%d, power main=%d external=%d display=%d", M5PAPER_DISPLAY_BUSY,
             M5PAPER_POWER_ENABLE, M5PAPER_EXTERNAL_POWER_ENABLE, M5PAPER_DISPLAY_POWER_ENABLE);
    ESP_LOGI(TAG, "dial: right=%d push=%d left=%d power=%d", M5PAPER_DIAL_RIGHT, M5PAPER_DIAL_PUSH,
             M5PAPER_DIAL_LEFT, M5PAPER_POWER_ENABLE);
    esp_err_t status = m5paper_buses_init(&buses);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "bus initialization failed: %s", esp_err_to_name(status));
        return status;
    }
    struct m5paper_it8951 display;
    status = m5paper_it8951_init(&buses, &display);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "IT8951 initialization failed: %s", esp_err_to_name(status));
        m5paper_buses_deinit(&buses);
        return status;
    }
    struct m5paper_it8951_info display_info;
    status = m5paper_it8951_system_info(&display, &display_info);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "IT8951 system-info query failed: %s", esp_err_to_name(status));
        m5paper_buses_deinit(&buses);
        return status;
    }
    status = m5paper_it8951_show_diagnostic(&display, &display_info);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "IT8951 diagnostic refresh failed: %s", esp_err_to_name(status));
        m5paper_buses_deinit(&buses);
        return status;
    }
    struct m5paper_gt911 touch;
    status = m5paper_gt911_init(&buses, &touch);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "GT911 probe failed: %s", esp_err_to_name(status));
        m5paper_buses_deinit(&buses);
        return status;
    }
    ESP_LOGI(TAG, "M5Paper display and touch diagnostics completed");
    m5paper_gt911_deinit(&touch);
    m5paper_it8951_deinit(&display);
    m5paper_buses_deinit(&buses);
    return ESP_OK;
}
