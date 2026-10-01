// Minimal placeholder firmware for the XTeink X4 (ESP32-C3).
//
// Not the xreader app -- there is no SSD1677 display driver or board
// abstraction for this board yet. This exists purely to replace whatever
// firmware is currently on the device with something that never sleeps, so
// the USB serial connection stays up while that real bring-up work happens.
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    printf("xteink_keepawake: running, sleep intentionally disabled\n");
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf("xteink_keepawake: alive\n");
    }
}
