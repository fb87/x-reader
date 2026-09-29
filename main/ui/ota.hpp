#pragma once
#include "gfx/framebuffer.hpp"
#include <stdint.h>
namespace xreader
{
namespace ui
{
enum ota_item_t : uint8_t
{
    ota_check_update,
    ota_install,
    ota_back,
    ota_item_count
};
void draw_ota(gfx::framebuffer_t* framebuffer, ota_item_t focus, const char* current_version,
              const char* available_version, const char* status, bool update_available);
bool ota_touch_item(uint16_t width, uint16_t height, uint16_t x, uint16_t y, ota_item_t* item);
} // namespace ui
} // namespace xreader
