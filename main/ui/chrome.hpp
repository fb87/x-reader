#pragma once

#include <stdint.h>

#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{
namespace chrome
{

// One cell of the bottom action bar.  Icon and label travel together so a cell
// can never be drawn with a label that does not match its icon.
struct footer_cell_t
{
    const char* label;
    gfx::icon_t icon;
};

uint16_t status_height(const gfx::framebuffer_t* framebuffer);
uint16_t indication_height(const gfx::framebuffer_t* framebuffer);
void set_battery_status(bool available, uint8_t percent);
void set_clock_visible(bool visible);

// Status bar: clock on the left, title centred, battery on the right.
void draw_status_bar(gfx::framebuffer_t* framebuffer, const char* title);

// Content header below the status bar, e.g. "Library            SD Card".
void draw_title_bar(gfx::framebuffer_t* framebuffer, const char* title, const char* trailing);
uint16_t title_height(layout::viewport_t viewport);

void draw_indication_bar(gfx::framebuffer_t* framebuffer, footer_cell_t left, footer_cell_t center,
                         footer_cell_t right);

} // namespace chrome
} // namespace ui
} // namespace xreader
