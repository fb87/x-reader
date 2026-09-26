#include "hardware_test.hpp"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gfx/font.hpp"
#include "input/input.hpp"

namespace xreader
{
namespace ui
{

namespace
{

static const char* const tag = "hardware_test";

static void draw_centered_text(gfx::framebuffer_t* framebuffer, uint16_t y, const char* text,
                               uint8_t scale, uint8_t value)
{
    const uint16_t text_width = static_cast<uint16_t>(strlen(text) * 6 * scale);
    const uint16_t x = text_width < framebuffer->width
                           ? static_cast<uint16_t>((framebuffer->width - text_width) / 2)
                           : 0;
    gfx::draw_text(framebuffer, x, y, text, scale, value);
}

static void draw_screen(gfx::framebuffer_t* framebuffer, bool touched, uint16_t touch_x,
                        uint16_t touch_y)
{
    gfx::clear(framebuffer, 0x00);
    gfx::draw_rect(framebuffer, 0, 0, framebuffer->width, framebuffer->height, 0x0f);
    draw_centered_text(framebuffer, 20, "TOUCH TEST", 3, 0x0f);
    gfx::fill_rect(framebuffer, 24, 76, framebuffer->width - 48, 2, 0x0f);
    draw_centered_text(framebuffer, 120, touched ? "TOUCH DETECTED" : "TAP SCREEN", 2, 0x0f);

    if (!touched)
    {
        return;
    }

    if (touch_x >= framebuffer->width)
    {
        touch_x = framebuffer->width - 1;
    }
    if (touch_y >= framebuffer->height)
    {
        touch_y = framebuffer->height - 1;
    }
    gfx::draw_rect(framebuffer, touch_x > 12 ? touch_x - 12 : 0, touch_y > 12 ? touch_y - 12 : 0,
                   25, 25, 0x0f);
    gfx::fill_rect(framebuffer, touch_x, 170, 2, framebuffer->height - 210, 0x06);
    gfx::fill_rect(framebuffer, 60, touch_y, framebuffer->width - 120, 2, 0x06);
}

static esp_err_t render(drivers::it8951e::device_t* display, gfx::framebuffer_t* framebuffer,
                        drivers::it8951e::refresh_mode_t mode)
{
    esp_err_t error = drivers::it8951e::write_image_4bpp(display, framebuffer->pixels, 0, 0,
                                                         framebuffer->width, framebuffer->height);
    if (error == ESP_OK)
    {
        error =
            drivers::it8951e::refresh(display, 0, 0, framebuffer->width, framebuffer->height, mode);
    }
    return error;
}

} // namespace

void run_hardware_test(drivers::it8951e::device_t* display, gfx::framebuffer_t* framebuffer,
                       QueueHandle_t events)
{
    bool touched = false;

    draw_screen(framebuffer, false, 0, 0);
    esp_err_t error = render(display, framebuffer, drivers::it8951e::refresh_gc16);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Initial hardware test render failed: %s", esp_err_to_name(error));
    }

    while (true)
    {
        input::event_t event = {};
        if (xQueueReceive(events, &event, portMAX_DELAY) == pdTRUE)
        {
            if (event.type == input::event_touch_down || event.type == input::event_touch_move)
            {
                touched = true;
                ESP_LOGI(tag, "touch x=%u y=%u", event.x, event.y);
                draw_screen(framebuffer, true, event.x, event.y);
                error = render(display, framebuffer, drivers::it8951e::refresh_du);
                if (error != ESP_OK)
                {
                    ESP_LOGE(tag, "Touch refresh failed: %s", esp_err_to_name(error));
                }
            }
            else if (event.type == input::event_touch_up && touched)
            {
                touched = false;
                ESP_LOGI(tag, "touch released");
                draw_screen(framebuffer, false, 0, 0);
                error = render(display, framebuffer, drivers::it8951e::refresh_du);
                if (error != ESP_OK)
                {
                    ESP_LOGE(tag, "Release refresh failed: %s", esp_err_to_name(error));
                }
            }
            else
            {
                ESP_LOGI(tag, "input event %u", event.type);
            }
        }
    }
}

} // namespace ui
} // namespace xreader
