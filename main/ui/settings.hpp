#pragma once

#include <stdint.h>

#include "gfx/framebuffer.hpp"
#include "ui/quick_settings.hpp"
#include "ui/settings_panel.hpp"

namespace xreader
{
namespace ui
{

// Settings is a hub.  The per-value reading and display controls live on their
// own screens (mockups 9 and 10); Book Manager and Book Sync moved here from the
// home launcher so Home can become the library list.
enum settings_item_t : uint8_t
{
    settings_display,
    settings_reading,
    settings_connectivity,
    settings_book_manager,
    settings_book_sync,
    settings_ota,
    settings_storage,
    settings_about,
    settings_back,
    settings_item_count,
};

void draw_settings(gfx::framebuffer_t* framebuffer, settings_item_t focus,
                   const quick_settings_values_t* values, int8_t footer_focus);
bool settings_touch_item(uint16_t display_width, uint16_t display_height, settings_item_t focus,
                         uint16_t x, uint16_t y, settings_item_t* item);

} // namespace ui
} // namespace xreader
