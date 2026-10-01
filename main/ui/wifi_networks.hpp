#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

struct wifi_network_view_t
{
    char ssid[33];
    int8_t rssi;
    bool secured;
};

void draw_wifi_networks(gfx::framebuffer_t* framebuffer, const wifi_network_view_t* networks,
                        uint8_t count, uint8_t focus, const char* status, int8_t footer_focus);
bool wifi_networks_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y,
                              uint8_t count, uint8_t* focus);

} // namespace ui
} // namespace xreader
