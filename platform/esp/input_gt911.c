#include "input_gt911.h"

#include <esp_log.h>

enum {
    GT911_ADDRESS_PRIMARY = 0x14,
    GT911_ADDRESS_SECONDARY = 0x5d,
    GT911_PRODUCT_ID = 0x8140,
    GT911_PRODUCT_ID_LENGTH = 4,
    GT911_STATUS = 0x814e,
    GT911_POINT_DATA = 0x8150,
    GT911_POINT_BYTES = 8,
};

static const char* const TAG = "x-reader-touch";

static esp_err_t read_register(i2c_master_dev_handle_t device, uint16_t address, uint8_t* data,
                               size_t length) {
    const uint8_t register_address[] = {(uint8_t)(address >> 8U), (uint8_t)(address & 0xffU)};
    return i2c_master_transmit_receive(device, register_address, sizeof(register_address), data,
                                       length, 100);
}

esp_err_t m5paper_gt911_init(const struct m5paper_buses* buses, struct m5paper_gt911* touch) {
    if (buses == NULL || buses->i2c == NULL || touch == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    touch->device = NULL;

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = GT911_ADDRESS_PRIMARY,
        .scl_speed_hz = 100000,
    };
    esp_err_t status = i2c_master_probe(buses->i2c, device_config.device_address, 100);
    if (status != ESP_OK) {
        device_config.device_address = GT911_ADDRESS_SECONDARY;
        status = i2c_master_probe(buses->i2c, device_config.device_address, 100);
    }
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "GT911 address probe failed: %s", esp_err_to_name(status));
        return status;
    }
    status = i2c_master_bus_add_device(buses->i2c, &device_config, &touch->device);
    if (status != ESP_OK) {
        return status;
    }

    uint8_t product_id[GT911_PRODUCT_ID_LENGTH] = {0};
    status = read_register(touch->device, GT911_PRODUCT_ID, product_id, sizeof(product_id));
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "GT911 product ID read failed: %s", esp_err_to_name(status));
        m5paper_gt911_deinit(touch);
        return status;
    }

    ESP_LOGI(TAG, "GT911 product ID: %c%c%c%c", product_id[0], product_id[1], product_id[2],
             product_id[3]);
    return ESP_OK;
}

void m5paper_gt911_deinit(struct m5paper_gt911* touch) {
    if (touch == NULL || touch->device == NULL) {
        return;
    }
    (void)i2c_master_bus_rm_device(touch->device);
    touch->device = NULL;
}

esp_err_t m5paper_gt911_read(struct m5paper_gt911* touch, struct m5paper_touch_point* points,
                             uint8_t* count) {
    if (touch == NULL || touch->device == NULL || points == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = 0;

    uint8_t status = 0;
    esp_err_t result = read_register(touch->device, GT911_STATUS, &status, sizeof(status));
    if (result != ESP_OK || (status & 0x80U) == 0U) {
        return result;
    }
    const uint8_t contacts = status & 0x0fU;
    if (contacts > M5PAPER_GT911_MAX_CONTACTS) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (contacts == 0U) {
        const uint8_t clear_status[] = {(uint8_t)(GT911_STATUS >> 8U),
                                        (uint8_t)(GT911_STATUS & 0xffU), 0};
        return i2c_master_transmit(touch->device, clear_status, sizeof(clear_status), 100);
    }

    uint8_t data[M5PAPER_GT911_MAX_CONTACTS * GT911_POINT_BYTES] = {0};
    result =
        read_register(touch->device, GT911_POINT_DATA, data, (size_t)contacts * GT911_POINT_BYTES);
    if (result == ESP_OK) {
        for (uint8_t index = 0; index < contacts; ++index) {
            const uint8_t* point = &data[(size_t)index * GT911_POINT_BYTES];
            points[index].x = (uint16_t)point[0] | ((uint16_t)point[1] << 8U);
            points[index].y = (uint16_t)point[2] | ((uint16_t)point[3] << 8U);
            points[index].size = (uint16_t)point[4] | ((uint16_t)point[5] << 8U);
            points[index].id = point[7];
        }
        *count = contacts;
        const uint8_t clear_status[] = {(uint8_t)(GT911_STATUS >> 8U),
                                        (uint8_t)(GT911_STATUS & 0xffU), 0};
        result = i2c_master_transmit(touch->device, clear_status, sizeof(clear_status), 100);
    }
    return result;
}
