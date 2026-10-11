#ifndef READER_ESP_PLATFORM_H
#define READER_ESP_PLATFORM_H

#include "../../core/include/platform.h"
#include <esp_err.h>

/*
 * Board pins are deliberately supplied by the verified M5Paper schematic
 * rather than guessed here. The platform remains unavailable until that data
 * is provided and the peripheral drivers are implemented.
 */
esp_err_t esp_platform_init(void);
esp_err_t esp_platform_create(struct platform* platform);
void esp_platform_destroy(struct platform* platform);

#endif
